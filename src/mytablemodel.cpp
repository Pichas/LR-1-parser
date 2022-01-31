#include "mytablemodel.h"

myTableModel::myTableModel(QList<QList<QString> > d, QObject *parent)
    : QAbstractTableModel(parent), m_data(d)
{
    m_hdata.reset(new QString[m_data.at(0).size()]);
}

int myTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_data.size(); // сделаем фиксированно 5 строк в таблице
    //если вы станете использовать скажем QList, то пишите return list.size();
}
int myTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    if (m_data.size() > 0)
        return m_data.at(0).size(); // количество колонок сделаем также фиксированным
    else
        return 0;
}

QVariant myTableModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {


        return QVariant((m_data.value(index.row(), {"0"})).value(index.column(), "c0"));
    }
    return QVariant();
}

bool myTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    Q_UNUSED(role);
    m_data[index.row()][index.column()] = value.toString();
    emit dataChanged(index, index);
    return true;
}

bool myTableModel::insertRows(int row, int count, const QModelIndex &parent)
{
    beginInsertRows(parent, row, row + count);

    endInsertRows();
    return true;
}

bool myTableModel::insertColumns(int column, int count, const QModelIndex &parent)
{
    beginInsertColumns(parent, column, column + count);

    endInsertColumns();
    return true;
}

QVariant myTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(role == Qt::DisplayRole){
        if(orientation == Qt::Vertical){
            return section;
        }

        if(orientation == Qt::Horizontal){
            if (section > m_data.at(0).size()) return QVariant();

            return m_hdata.data()[section];
        }
    }
    return QVariant();
}

bool myTableModel::setHeaderData(int section, Qt::Orientation orientation, const QVariant &value, int role)
{
    if(role == Qt::EditRole){
        if(orientation == Qt::Horizontal){
            if (section > m_data.at(0).size()) return false;
            m_hdata.data()[section] = value.toString();
        }
    }
    emit headerDataChanged(orientation, section, section);
    return true;
}
