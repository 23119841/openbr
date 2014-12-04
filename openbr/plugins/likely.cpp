#include <likely.h>
#include <likely/opencv.hpp>

#include "openbr/core/opencvutils.h"
#include "openbr_internal.h"

namespace br
{

/*!
 * \ingroup transforms
 * \brief Generic interface to Likely JIT compiler
 *
 * www.liblikely.org
 * \author Josh Klontz \cite jklontz
 */
class LikelyTransform : public UntrainableTransform
{
    Q_OBJECT
    Q_PROPERTY(QString kernel READ get_kernel WRITE set_kernel RESET reset_kernel STORED false)
    BR_PROPERTY(QString, kernel, "")

    likely_const_env env;
    void *function;

    ~LikelyTransform()
    {
        likely_release_env(env);
    }

    void init()
    {
        likely_release_env(env);
        const likely_ast ast = likely_lex_and_parse(qPrintable(kernel), likely_file_lisp);
        const likely_const_env parent = likely_standard(NULL);
        env = likely_eval(ast, parent, NULL, NULL);
        likely_release_env(parent);
        likely_release_ast(ast);
        function = likely_compile(env->expr, NULL, 0);
    }

    void project(const Template &src, Template &dst) const
    {
        const likely_const_mat srcl = likelyFromOpenCVMat(src);
        const likely_const_mat dstl = reinterpret_cast<likely_mat (*)(likely_const_mat)>(function)(srcl);
        dst = likelyToOpenCVMat(dstl);
        likely_release_mat(dstl);
        likely_release_mat(srcl);
    }

public:
    LikelyTransform()
    {
        env = NULL;
    }
};

BR_REGISTER(Transform, LikelyTransform)

/*!
 * \ingroup formats
 * \brief Likely matrix format
 *
 * www.liblikely.org
 * \author Josh Klontz \cite jklontz
 */
class lmatFormat : public Format
{
    Q_OBJECT

    Template read() const
    {
        const likely_const_mat m = likely_read(qPrintable(file.name), likely_file_guess);
        const Template result(likelyToOpenCVMat(m));
        likely_release_mat(m);
        return result;
    }

    void write(const Template &t) const
    {
        const likely_const_mat m = likelyFromOpenCVMat(t);
        likely_write(m, qPrintable(file.name));
        likely_release_mat(m);
    }
};

BR_REGISTER(Format, lmatFormat)

/*!
 * \ingroup formats
 * \brief Likely matrix format
 *
 * www.liblikely.org
 * \author Josh Klontz \cite jklontz
 */
class lmatGallery : public Gallery
{
    Q_OBJECT
    QList<cv::Mat> mats;

    ~lmatGallery()
    {
        const likely_const_mat m = likelyFromOpenCVMat(OpenCVUtils::toMatByRow(mats));
        likely_write(m, qPrintable(file.name));
        likely_release_mat(m);
    }

    TemplateList readBlock(bool *done)
    {
        *done = true;
        qFatal("Not supported.");
    }

    void write(const Template &t)
    {
        mats.append(t);
    }
};

BR_REGISTER(Gallery, lmatGallery)

} // namespace br

#include "likely.moc"
