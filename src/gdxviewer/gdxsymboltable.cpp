#include "gdxsymboltable.h"
#include "exception.h"

namespace gams {
namespace studio {
namespace gdxviewer {

GdxSymbolTable::GdxSymbolTable(gdxHandle_t gdx, QObject *parent)
    : QAbstractTableModel(parent), mGdx(gdx)
{
    gdxSystemInfo(mGdx, &mSymbolCount, &mUelCount);
    loadUel2Label();
    loadGDXSymbols();

    mHeaderText.append("Entry");
    mHeaderText.append("Name");
    mHeaderText.append("Type");
    mHeaderText.append("Dimension");
    mHeaderText.append("Nr Records");
}

GdxSymbolTable::~GdxSymbolTable()
{
    for(auto gdxSymbol : mGdxSymbols)
        delete gdxSymbol;
}

QVariant GdxSymbolTable::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role == Qt::DisplayRole)
    {
        if (orientation == Qt::Horizontal)
            if (section < mHeaderText.size())
                return mHeaderText.at(section);
    }
    return QVariant();
}

int GdxSymbolTable::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return mGdxSymbols.size();
}

int GdxSymbolTable::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return mHeaderText.size();
}

QVariant GdxSymbolTable::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();
    if (role == Qt::DisplayRole)
        switch(index.column())
        {
        case 0: return mGdxSymbols.at(index.row())->nr(); break;
        case 1: return mGdxSymbols.at(index.row())->name(); break;
        case 2: return mGdxSymbols.at(index.row())->type(); break;
        case 3: return mGdxSymbols.at(index.row())->dim(); break;
        case 4: return mGdxSymbols.at(index.row())->recordCount(); break;
        }
    return QVariant();
}

void GdxSymbolTable::loadGDXSymbols()
{
    for(int i=1; i<mSymbolCount+1; i++)
    {
        char symName[GMS_SSSIZE];
        char explText[GMS_SSSIZE];
        int dimension = 0;
        int type = 0;
        gdxSymbolInfo(mGdx, i, symName, &dimension, &type);
        int recordCount = 0;
        int userInfo = 0;
        gdxSymbolInfoX (mGdx, i, &recordCount, &userInfo, explText);
        mGdxSymbols.append(new GdxSymbol(mGdx, &mUel2Label, i, QString(symName), dimension, type, userInfo, recordCount, QString(explText)));
    }
}

void GdxSymbolTable::loadUel2Label()
{
    char label[GMS_SSSIZE];
    int map;
    for(int i=0; i<=mUelCount; i++)
    {
        gdxUMUelGet(mGdx, i, label, &map);
        mUel2Label.append(QString(label));
    }
}

void GdxSymbolTable::reportIoError(int errNr, QString message)
{
    //TODO(CW): proper Exception message and remove qDebug
    qDebug() << "**** Fatal I/O Error = " << errNr << " when calling " << message;
    throw Exception();
}

QList<GdxSymbol *> GdxSymbolTable::gdxSymbols() const
{
    return mGdxSymbols;
}

QString GdxSymbolTable::uel2Label(int uel)
{
    return mUel2Label.at(uel);
}

} // namespace gdxviewer
} // namespace studio
} // namespace gams
