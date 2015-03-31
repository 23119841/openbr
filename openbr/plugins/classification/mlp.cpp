/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright 2012 The MITRE Corporation                                      *
 *                                                                           *
 * Licensed under the Apache License, Version 2.0 (the "License");           *
 * you may not use this file except in compliance with the License.          *
 * You may obtain a copy of the License at                                   *
 *                                                                           *
 *     http://www.apache.org/licenses/LICENSE-2.0                            *
 *                                                                           *
 * Unless required by applicable law or agreed to in writing, software       *
 * distributed under the License is distributed on an "AS IS" BASIS,         *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  *
 * See the License for the specific language governing permissions and       *
 * limitations under the License.                                            *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <opencv2/ml/ml.hpp>

#include <openbr/plugins/openbr_internal.h>
#include <openbr/core/opencvutils.h>

using namespace cv;

namespace br
{

/*!
 * \ingroup transforms
 * \brief Wraps OpenCV's multi-layer perceptron framework
 * \author Scott Klum \cite sklum
 * \brief http://docs.opencv.org/modules/ml/doc/neural_networks.html
 */
class MLPTransform : public Transform
{
    Q_OBJECT

    Q_ENUMS(Kernel)
    Q_PROPERTY(Kernel kernel READ get_kernel WRITE set_kernel RESET reset_kernel STORED false)
    Q_PROPERTY(float alpha READ get_alpha WRITE set_alpha RESET reset_alpha STORED false)
    Q_PROPERTY(float beta READ get_beta WRITE set_beta RESET reset_beta STORED false)
    Q_PROPERTY(QStringList inputVariables READ get_inputVariables WRITE set_inputVariables RESET reset_inputVariables STORED false)
    Q_PROPERTY(QStringList outputVariables READ get_outputVariables WRITE set_outputVariables RESET reset_outputVariables STORED false)
    Q_PROPERTY(QList<int> neuronsPerLayer READ get_neuronsPerLayer WRITE set_neuronsPerLayer RESET reset_neuronsPerLayer STORED false)

public:

    enum Kernel { Identity = CvANN_MLP::IDENTITY,
                  Sigmoid = CvANN_MLP::SIGMOID_SYM,
                  Gaussian = CvANN_MLP::GAUSSIAN};

private:
    BR_PROPERTY(Kernel, kernel, Sigmoid)
    BR_PROPERTY(float, alpha, 1)
    BR_PROPERTY(float, beta, 1)
    BR_PROPERTY(QStringList, inputVariables, QStringList())
    BR_PROPERTY(QStringList, outputVariables, QStringList())
    BR_PROPERTY(QList<int>, neuronsPerLayer, QList<int>() << 1 << 1)

    CvANN_MLP mlp;

    void init()
    {
        if (kernel == Gaussian)
            qWarning("The OpenCV documentation warns that the Gaussian kernel, \"is not completely supported at the moment\"");

        Mat layers = Mat(neuronsPerLayer.size(), 1, CV_32SC1);
        for (int i=0; i<neuronsPerLayer.size(); i++)
            layers.row(i) = Scalar(neuronsPerLayer.at(i));

        mlp.create(layers,kernel, alpha, beta);
    }

    void train(const TemplateList &data)
    {
        Mat _data = OpenCVUtils::toMat(data.data());

        // Assuming data has n templates
        // _data needs to be n x size of input layer
        // Labels needs to be a n x outputs matrix
        // For the time being we're going to assume a single output
        Mat labels = Mat::zeros(data.size(),inputVariables.size(),CV_32F);
        for (int i=0; i<inputVariables.size(); i++)
            labels.col(i) += OpenCVUtils::toMat(File::get<float>(data, inputVariables.at(i)));

        mlp.train(_data,labels,Mat());

        if (Globals->verbose)
            for (int i=0; i<neuronsPerLayer.size(); i++) qDebug() << *mlp.get_weights(i);
    }

    void project(const Template &src, Template &dst) const
    {
        dst = src;

        // See above for response dimensionality
        Mat response(outputVariables.size(), 1, CV_32FC1);
        mlp.predict(src.m().reshape(1,1),response);

        // Apparently mlp.predict reshapes the response matrix?
        for (int i=0; i<outputVariables.size(); i++) dst.file.set(outputVariables.at(i),response.at<float>(0,i));
    }

    void load(QDataStream &stream)
    {
        OpenCVUtils::loadModel(mlp, stream);
    }

    void store(QDataStream &stream) const
    {
        OpenCVUtils::storeModel(mlp, stream);
    }
};

BR_REGISTER(Transform, MLPTransform)

/*!
 * \ingroup classifiers
 * \brief Wraps OpenCV's multi-layer perceptron framework
 * \author Scott Klum \cite sklum
 * \author Jordan Cheney \cite JordanCheney
 * \brief http://docs.opencv.org/modules/ml/doc/neural_networks.html
 */
class MLPClassifier : public Classifier
{
    Q_OBJECT

    Q_ENUMS(Kernel)

    Q_PROPERTY(br::Representation *representation READ get_representation WRITE set_representation RESET reset_representation STORED false)
    Q_PROPERTY(Kernel kernel READ get_kernel WRITE set_kernel RESET reset_kernel STORED false)
    Q_PROPERTY(float alpha READ get_alpha WRITE set_alpha RESET reset_alpha STORED false)
    Q_PROPERTY(float beta READ get_beta WRITE set_beta RESET reset_beta STORED false)
    Q_PROPERTY(QList<int> hiddenLayerNeurons READ get_hiddenLayerNeurons WRITE set_hiddenLayerNeurons RESET reset_hiddenLayerNeurons STORED false)

public:

    enum Kernel { Identity = CvANN_MLP::IDENTITY,
                  Sigmoid = CvANN_MLP::SIGMOID_SYM,
                  Gaussian = CvANN_MLP::GAUSSIAN};

private:
    BR_PROPERTY(br::Representation*, representation, NULL)
    BR_PROPERTY(Kernel, kernel, Sigmoid)
    BR_PROPERTY(float, alpha, 1)
    BR_PROPERTY(float, beta, 1)
    BR_PROPERTY(QList<int>, hiddenLayerNeurons, QList<int>()) // Number of neurons in the middle layers

    CvANN_MLP mlp;

    void init()
    {
        if (kernel == Gaussian)
            qWarning("The OpenCV documentation warns that the Gaussian kernel, \"is not completely supported at the moment\"");

        Mat layers = Mat(hiddenLayerNeurons.size() + 2, 1, CV_32SC1);
        layers.row(0) = representation->numFeatures();
        layers.row(layers.rows - 1) = 1;
        for (int i=0; i<hiddenLayerNeurons.size(); i++)
            layers.at<int>(i+1) = hiddenLayerNeurons[i];

        mlp.create(layers, kernel, alpha, beta);
    }

    bool train(const QList<Mat> &_images, const QList<float> &_labels)
    {
        Mat data(_images.size(), representation->numFeatures(), CV_32F);
        for (int i = 0; i < _images.size(); i++)
            data.row(i) = representation->evaluate(_images[i]);
        Mat labels = OpenCVUtils::toMat(_labels);

        int iterations = mlp.train(data,labels,Mat());

        if (iterations == 0)
            return false;
        return true;
    }

    float classify(const Mat &image) const
    {
        Mat response(1, 1, CV_32FC1);
        mlp.predict(representation->evaluate(image), response);
        return response.at<float>(0, 0);
    }

    Mat preprocess(const Mat &image) const
    {
        return representation->preprocess(image);
    }

    Size windowSize() const
    {
        return representation->windowSize();
    }

    void load(QDataStream &stream)
    {
        OpenCVUtils::loadModel(mlp, stream);
    }

    void store(QDataStream &stream) const
    {
        OpenCVUtils::storeModel(mlp, stream);
    }
};

BR_REGISTER(Classifier, MLPClassifier)

} // namespace br

#include "classification/mlp.moc"
