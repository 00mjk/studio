#ifndef SYNTAXFORMATS_H
#define SYNTAXFORMATS_H

#include <QtGui>

namespace gams {
namespace studio {

enum class SyntaxState {
    Standard,
    Directive,
    CommentLine,
    CommentBlock,
    CommentEndline,
    CommentInline,
    CommentMargin,

    StateCount
};
typedef QList<SyntaxState> SyntaxStates;

class SyntaxAbstract;

struct SyntaxBlock
{
    SyntaxBlock(SyntaxAbstract* _syntax=nullptr, SyntaxState _next=SyntaxState::Standard, int _start=0, int _end=0, bool _error=false)
        : syntax(_syntax), next(_next), start(_start), end(_end), error(_error) { }
    SyntaxAbstract *syntax;
    SyntaxState next;
    int start;
    int end;
    bool error;
    int length() { return end-start; }
    bool isValid() { return syntax; }
};

/// \brief An abstract class to be used inside the <c>SyntaxHighlighter</c>.
class SyntaxAbstract
{
public:
    virtual ~SyntaxAbstract() {}
    virtual SyntaxState state() = 0;
    virtual SyntaxBlock find(SyntaxState entryState, const QString &line, int index) = 0;
    virtual SyntaxStates subStates() { return mSubStates; }
    virtual QTextCharFormat& charFormat() { return mCharFormat; }
    virtual QTextCharFormat charFormatError();
    virtual void copyCharFormat(QTextCharFormat charFormat) {
        mCharFormat = charFormat;
    }
protected:
    QTextCharFormat mCharFormat;
    SyntaxStates mSubStates;
};


/// \brief Defines the syntax for standard code.
class SyntaxStandard : public SyntaxAbstract
{
public:
    SyntaxStandard();
    SyntaxState state() override { return SyntaxState::Standard; }
    SyntaxBlock find(SyntaxState entryState, const QString &line, int index) override;
};


/// \brief Defines the syntax for a directive.
class SyntaxDirective : public SyntaxAbstract
{
public:
    SyntaxDirective(QChar directiveChar = '$');
    SyntaxState state() override { return SyntaxState::Directive; }
    SyntaxBlock find(SyntaxState entryState, const QString &line, int index) override;
private:
    QRegularExpression mRex;
    QStringList mDirectives;
    QHash<QString, SyntaxState> mSpecialStates;
};


/// \brief Defines the syntax for a single comment line.
class SyntaxCommentLine: public SyntaxAbstract
{
public:
    SyntaxCommentLine(QChar commentChar = '*');
    SyntaxState state() override { return SyntaxState::CommentLine; }
    SyntaxBlock find(SyntaxState entryState, const QString &line, int index) override;
private:
    QChar mCommentChar;
};

/// \brief Defines the syntax for a single comment line.
class SyntaxCommentBlock: public SyntaxAbstract
{
public:
    SyntaxCommentBlock();
    SyntaxState state() override { return SyntaxState::CommentBlock; }
    SyntaxBlock find(SyntaxState entryState, const QString &line, int index) override;
};

} // namespace studio
} // namespace gams

#endif // SYNTAXFORMATS_H
