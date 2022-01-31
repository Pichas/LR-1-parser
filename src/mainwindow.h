#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QRegularExpression>
#include <QDebug>
#include <mytablemodel.h>
#include <QModelIndex>
#include <QStringListModel>
#include <QStack>
#include <QTimer>
#include <QSharedDataPointer>
#include <QElapsedTimer>



QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

typedef struct d{
    int level;
    QString type;
    QString name;
    QList<QPair<QString, QString>>args;
    bool isProto;

    bool operator==(const d& r)
    {

        bool isArgsTypesRepeat = true;


        if (args.size() != r.args.size()) isArgsTypesRepeat = false;; //если разное количество аргументов, сразу выходим



        for (int i = 0 ; i < qMin(args.size(), r.args.size()) ; i ++){ //пройтись по всем
            if(args.at(i).first != r.args.at(i).first){
                isArgsTypesRepeat = false;
                break;
            }
        }

        //проверить пустые функции
        if ((args.size() == 0 || (args.size() == 1 && args.at(0).first == "void")) &&
            (r.args.size() == 0 || (r.args.size() == 1 && r.args.at(0).first == "void"))){

            isArgsTypesRepeat = true;
        }

        return (this->level == r.level && //на одинаковом уровне
                this->name == r.name && //одинаковое имя
                isArgsTypesRepeat && //типы аргументов в одиноковой последовательности
                (this->isProto == false && //обе функции
                r.isProto == false)); // являются реализациями
    }

} Def;



typedef struct rule{
    QString left;
    QString right;
    bool operator==(const rule& r)
    {
        return (this->left == r.left &&
                this->right == r.right);
    }
} Rule;

typedef struct sit{
    Rule rule;
    int  pos;
    QString term;

    bool operator==(const sit& r)
    {
        return (this->rule == r.rule &&
                this->pos == r.pos &&
                this->term == r.term);
    }
} Situation;

typedef struct st {
    int name;
    QList<Situation> situations;
    bool operator==(const st& r)
    {
        return (this->situations == r.situations);
    }
} State;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void work();


private:
    Ui::MainWindow *ui;
    QString calcPosition(int curent);

    bool buildTable();

    void fillGrammar();
    void parseN();
    void parseT();
    void findFirst();
    QStringList ffRecursive(QString PFirst);

    void generateStates();
    void generateGoTo(State &state);
    void closure(State &state);
    QStringList pickTermSimbol(QString b, QString aa);

    void generateReserveWords();

    void parseInput();
    bool checkInput(QString templ, QString input);
    QString inputToPattern(QString c); //замена входного сивола на шаблон РЕ
    int checkReservedWords(QString &in, int pos, int posInWord);

    void impActionE(QString last, QString ch); //действия для терминалов
    void impActionN(int rn); //действия для нетерминалов



    //разбор грамматики
    QList<Rule> m_P; //грамматика
    QList<Rule> m_P_RAW; //грамматика
    QStringList m_N; //все нетерминалы
    QStringList m_E; //все терминалы
    QStringList m_A; //все действия
    QString m_S; //старт
    QList<QPair<QString, QStringList>> m_starts; //множество стартовых символов
    QList<State> m_states; //варианты
    QList<QStringList> m_goto; //переходы

    int m_deep = 0; //просмотр глубины рекурсии


    QList<QList<QString>> m_data; //таблица разбора
    QMap<QString, int> m_columns; //карта столбцов
    QScopedPointer<myTableModel>m_dataTableModel; //модель для отображения таблицы разбора

    QList<QList<QString>> m_process; //процесс разбора
    QScopedPointer<myTableModel> m_processModel; //модель для отображениея процесса разбора


    //встроенные действия
    QStringList m_reservedWords; //служебные слова (они же терминалы не из диапазона символов)
    QStringList m_namespace; //пространства имен (могут быть разными словами описаны)

    int m_level; //уровень вложенности
    QString m_type; //тип
    QString m_name; //имя
    QList<QPair<QString, QString>>m_args; //аргументы

    QList<Def> m_allDefs; //все объявления
    QString m_checkError = ""; //резултат с ошибкой при проверке







};
#endif // MAINWINDOW_H
