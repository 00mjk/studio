/*
 * This file is part of the GAMS Studio project.
 *
 * Copyright (c) 2017-2020 GAMS Software GmbH <support@gams.com>
 * Copyright (c) 2017-2020 GAMS Development Corp. <support@gams.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef SYNTAXIDENTIFIER_H
#define SYNTAXIDENTIFIER_H

#include "syntaxformats.h"

namespace gams {
namespace studio {
namespace syntax {

class SyntaxIdentifier : public SyntaxAbstract
{
public:
    SyntaxIdentifier();
    SyntaxBlock find(const SyntaxKind entryKind, int flavor, const QString &line, int index) override;
    SyntaxBlock validTail(const QString &line, int index, int flavor, bool &hasContent) override;
private:
    int identChar(const QChar &c) const;
};

class SyntaxIdentifierDim : public SyntaxAbstract
{
    const QString mDelimiters;
public:
    SyntaxIdentifierDim();
    SyntaxBlock find(const SyntaxKind entryKind, int flavor, const QString &line, int index) override;
    SyntaxBlock validTail(const QString &line, int index, int flavor, bool &hasContent) override;
    int maxNesting() override { return 1; }
};

class SyntaxIdentifierDimEnd : public SyntaxAbstract
{
    const QString mDelimiters;
public:
    SyntaxIdentifierDimEnd();
    SyntaxBlock find(const SyntaxKind entryKind, int flavor, const QString &line, int index) override;
    SyntaxBlock validTail(const QString &line, int index, int flavor, bool &hasContent) override;
};

class SyntaxIdentDescript : public SyntaxAbstract
{
public:
    SyntaxIdentDescript();
    SyntaxBlock find(const SyntaxKind entryKind, int flavor, const QString &line, int index) override;
    SyntaxBlock validTail(const QString &line, int index, int flavor, bool &hasContent) override;
};

class SyntaxIdentAssign : public SyntaxAbstract
{
public:
    SyntaxIdentAssign(SyntaxKind kind);
    SyntaxBlock find(const SyntaxKind entryKind, int flavor, const QString &line, int index) override;
    SyntaxBlock validTail(const QString &line, int index, int flavor, bool &hasContent) override;
};

class AssignmentLabel: public SyntaxAbstract
{
public:
    AssignmentLabel();
    SyntaxBlock find(const SyntaxKind entryKind, int flavor, const QString &line, int index) override;
    SyntaxBlock validTail(const QString &line, int index, int flavor, bool &hasContent) override;
};

class AssignmentValue: public SyntaxAbstract
{
public:
    AssignmentValue();
    SyntaxBlock find(const SyntaxKind entryKind, int flavor, const QString &line, int index) override;
    SyntaxBlock validTail(const QString &line, int index, int flavor, bool &hasContent) override;
};

class SyntaxTableAssign : public SyntaxAbstract
{
public:
    SyntaxTableAssign(SyntaxKind kind);
    SyntaxBlock find(const SyntaxKind entryKind, int flavor, const QString &line, int index) override;
    SyntaxBlock validTail(const QString &line, int index, int flavor, bool &hasContent) override;
};


} // namespace syntax
} // namespace studio
} // namespace gams

#endif // SYNTAXIDENTIFIER_H
