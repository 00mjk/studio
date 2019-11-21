#include "gdxdiffprocess.h"
#include <QDebug>
#include <QDir>
#include <locators/abstractsystemlogger.h>
#include "locators/sysloglocator.h"

namespace gams {
namespace studio {
namespace gdxdiffdialog {

GdxDiffProcess::GdxDiffProcess(QObject *parent)
    : AbstractProcess("gdxdiff", parent)
{

}

void GdxDiffProcess::execute()
{
    QStringList args;
    args << QDir::toNativeSeparators(mInput1);
    args << QDir::toNativeSeparators(mInput2);
    if (!mDiff.isEmpty())
        args << QDir::toNativeSeparators(mDiff);
    args << "Field=" + mFieldToCompare;
    if (!mEps.isEmpty())
        args << "Eps=" + mEps;
    if (!mRelEps.isEmpty())
        args << "RelEps=" + mRelEps;
    if (mFieldOnly)
        args << "FldOnly";
    if (mDiffOnly)
        args << "DiffOnly";
    if (mIgnoreSetText)
        args << "SetDesc=0";
    connect(this, &AbstractProcess::newStdChannelData, this, &GdxDiffProcess::appendSystemLog);
    mProcess.setWorkingDirectory(mWorkingDir);
    mProcess.start(nativeAppPath(), args);
    mProcess.waitForFinished();
}

void GdxDiffProcess::setInput1(const QString &input1)
{
    mInput1 = input1;
}

void GdxDiffProcess::setInput2(const QString &input2)
{
    mInput2 = input2;
}

void GdxDiffProcess::setDiff(const QString &diff)
{
    mDiff = diff;
}

void GdxDiffProcess::setIgnoreSetText(bool ignoreSetText)
{
    mIgnoreSetText = ignoreSetText;
}

void GdxDiffProcess::setDiffOnly(bool diffOnly)
{
    mDiffOnly = diffOnly;
}

void GdxDiffProcess::setFieldOnly(bool fieldOnly)
{
    mFieldOnly = fieldOnly;
}

void GdxDiffProcess::setFieldToCompare(const QString &fieldToCompare)
{
    mFieldToCompare = fieldToCompare;
}

void GdxDiffProcess::setEps(const QString &eps)
{
    mEps = eps;
}

void GdxDiffProcess::setRelEps(const QString &relEps)
{
    mRelEps = relEps;
}

void GdxDiffProcess::setWorkingDir(const QString &workingDir)
{
    mWorkingDir = workingDir;
}

QString GdxDiffProcess::diffFile() const
{
    return mDiffFile;
}

void GdxDiffProcess::appendSystemLog(const QString &text)
{
    SysLogLocator::systemLog()->append(text, LogMsgType::Info);
    if (text.contains("Output:")) {
        mDiffFile = text.split("Output:").last().trimmed();
        if (QFileInfo(mDiffFile).isRelative())
            mDiffFile = QDir::cleanPath(mWorkingDir + QDir::separator() + mDiffFile);
    }
}

} // namespace gdxdiffdialog
} // namespace studio
} // namespace gams
