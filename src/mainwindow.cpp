#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->pbWork, &QPushButton::clicked, this, &MainWindow::work);

    //показать в строке состояния текущую позицию курсора
    connect(ui->pteInput, &QPlainTextEdit::cursorPositionChanged, [&]{
        statusBar()->showMessage(calcPosition(ui->pteInput->textCursor().position()));
    });

    this->showMaximized();
}

QString MainWindow::calcPosition(int curent)
{
    QString src = ui->pteInput->toPlainText();
    int row = 0, column = 0, cursor = 0;

    //посчитать номер строки
    while (src.size() > cursor && cursor < curent){
        if(src[cursor++] == '\n') {
            row++;
            column = 0; //запомнить позицию начала строки
        }else{
            column++;
        }
    }

    return " [Строка: " + QString::number(row) + "].[Позиция: " + QString::number(column) + "]";
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::work()
{
    QSharedPointer<QTimer> t(new QTimer,[&](QTimer *o){
        o->stop();
        o->deleteLater();
        ui->pbWork->setEnabled(true);
        ui->pbWork->setText("Работать");
    });
    QElapsedTimer et;
    et.start();

    connect(t.data(), &QTimer::timeout, [&]{
        ui->pbWork->setText("Работаем! Время выполнения[с]: " + QString::number(et.elapsed()/1000));

        if (et.elapsed() / 1000 > 10)
            ui->pbWork->setText("Все еще работаем, видимо компьютер слабый ... Время выполнения[с]: " + QString::number(et.elapsed()/1000));

        if (et.elapsed() / 1000 > 15)
            ui->pbWork->setText("Может попробовать запустить на компьютере помощнее? Время выполнения[с]:  " + QString::number(et.elapsed()/1000));

        if (et.elapsed() / 1000 > 30)
            ui->pbWork->setText("Даже спустя столько времени мы все еще работаем! Время выполнения[с]:  " + QString::number(et.elapsed()/1000));

    });

    t->start(1000);
    ui->pbWork->setEnabled(false);
    ui->leResult->setText("");

    qApp->processEvents();

    if(buildTable())
        parseInput();

    ui->tvTable->resizeColumnsToContents();
    ui->tvTable->resizeRowsToContents();
    ui->lvSteps->resizeColumnsToContents();
    ui->lvSteps->resizeRowsToContents();

}

bool MainWindow::buildTable()
{
    m_data.clear();
    m_columns.clear();

    fillGrammar(); //заполнить грамматику
    parseN(); //определить нетерминалы
    parseT(); //определить терминалы
    findFirst(); //найти первые симаволы
    generateStates(); //сгенерировать состояния начиная со старта

    generateReserveWords();//сгенерировать ключевые слова

    QStringList row; //пустышка для заполнения

    //сопоставить имена столбцов с номерами
    int c = 0;
    for (auto v : m_E) m_columns[v] = c++;

    for (auto v : m_N) m_columns[v] = c++;

    //создать строку нужной длины
    for (int i = 0 ; i < m_columns.size() ; i++)
        row << "";

    //создать нужное колво строк
    for (int r = 0 ; r < m_states.size() ; r++)
        m_data.append(row);

    //заполнить таблицу  свертками
    for (auto state : m_states){
        for (auto sit : state.situations){
            if (sit.rule.right.split(" ").size() <= sit.pos){
                if(m_P.indexOf(sit.rule)){
                    if (m_data[state.name][m_columns.value(sit.term)].contains("R")){ //если в ячейке уже есть свертка, то у нас конфликт
                        ui->leResult->setText("Ошибка разбора грамматики. Конфликт свертка-свертка.");
                        return false;
                    }
                    m_data[state.name][m_columns.value(sit.term)] = "R" + QString::number(m_P.indexOf(sit.rule));

                }else{
                    m_data[state.name][m_columns.value(sit.term)] = "HALT";
                }
            }
        }
    }

    //заполнить таблицу  сдвигами. При конфликте - пишем поверх
    for (auto row : m_goto){
        if (m_data[row[0].toInt()][m_columns.value(row[1])].contains("S")){
            ui->leResult->setText("Ошибка разбора грамматики. Конфликт сдвиг-сдвиг.");
            return false;
        }
        m_data[row[0].toInt()][m_columns.value(row[1])] = "S" + row[2];
    }

    //отобразить таблицу
    m_dataTableModel.reset(new myTableModel(m_data, nullptr));
    ui->tvTable->clearSpans();
    ui->tvTable->setModel(m_dataTableModel.data());

    //заголовки
    for (int i = 0 ; i < m_columns.size() ; i++)
        m_dataTableModel->setHeaderData(i, Qt::Horizontal, m_columns.key(i));

    return true;
}


void MainWindow::fillGrammar()
{
    m_P.clear();
    m_P_RAW.clear();

    QStringList rows = ui->pteGram->toPlainText().trimmed().split("\n");

    //сделать стартовую позицию
    m_S = rows.at(0).split("->").at(0).trimmed() + "'";
    rows.prepend(QString(m_S + " -> " + m_S.chopped(1)));

    const QRegularExpression re(R"(<.*?>)"); //найти соответствия

    for (auto row : rows){
        if (!row.contains("->")) continue; //если нет символа, то пропустить строку
        if (row.split("->").size() > 2) continue; //если два символа и больше то пропустить

        QString left = row.split("->").at(0).simplified();
        QString right = row.split("->").at(1).simplified();
        for (auto multiRight : right.split("|", Qt::SkipEmptyParts)){
            m_P_RAW += {left, multiRight.simplified()}; //добавить полные правила с действиями для дальнейшего разбора
            multiRight.replace(re, ""); //забирать действия
            m_P += {left, multiRight.simplified()/*, actions*/}; // добавить их к правилам грамматики для дальнейшего разбора во время сверток?
        }
    }

    QString out;
    for (auto rule : m_P){
            out += "(" + rule.left + ", " + rule.right + ") \n"; //a:" + rule.actions.join(", ") + "
    }

    ui->pteOutput->setPlainText(out);
    qApp->processEvents();
}



void MainWindow::parseN()
{
    m_N.clear();
    for(auto rule : m_P){ //пройтись по всем правилаам
        m_N << rule.left; //добавить все нетерминалы слева
    }
    m_N.removeDuplicates(); //удалить дубликаты
    qApp->processEvents();
}

void MainWindow::parseT()
{
    m_E.clear();

    for(auto rule : m_P){ //посмотреть все правила
        for (auto word : rule.right.split(" ")){ //все слова у каждой кальтернативы
            if (m_N.contains(word)) continue; //если нетерминал то пропустиь
            m_E << word;
        }
    }

    m_E.removeDuplicates(); //удалить дубликаты
    m_E.removeOne(""); //удалить пустое
    m_E << "$"; //добавить конец разбора
    qApp->processEvents();
}

void MainWindow::findFirst()
{
    m_starts.clear();

    m_deep = 0; //сбросить счетчик
    for(auto n : m_N){ //заполнить массив стартовых символов
        m_starts.append({n, ffRecursive(n)}); //рекурсивно пройтись по всем элементам
    }

    //вывод результата
    QString stringStarts;
    for (auto start : m_starts){
        stringStarts += "S(" + start.first +") = {" + start.second.join(",") + "}\n";
    }

    ui->pteOutput->setPlainText(ui->pteOutput->toPlainText() + "\n" + stringStarts);
    qApp->processEvents();
}

QStringList MainWindow::ffRecursive(QString PFirst)
{
    if (++m_deep > m_P.size()){
        ui->leResult->setText("Ошибка разбора грамматики. Невозможно определить первичные симаолы.");
        return {};
    }

    QStringList first;
    if (m_N.contains(PFirst)){ //если первый символ нетерминал
        //продолжить поиск

        //определить из грамматики все правила
        //где в левой части стоит этот нетерминал и выделить все первые элементы справа

        for (auto rule : m_P){ //обработать все правила
            if(PFirst != rule.left)continue; //если слева нужный нетерминал, то
            QString f = rule.right.split(" ").value(0, ""); //первый элемент
            if (f == PFirst) continue; //если символ совпадает, то пропустить правило
            //если символ уже есть, то выйти
            if (m_N.contains(f))//если первый элемент нетерминал, рекурсивно обработать
                first += ffRecursive(f);
            else //иначе вернуть значение терминала
                first += f;
        }
    }
    first.removeDuplicates();

    --m_deep;
    qApp->processEvents();
    return first;
}

void MainWindow::closure(State &state)
{
    for (int sitNum = 0; sitNum < state.situations.count() ; sitNum++ ) { //обрабатывать все ситуации в текущем состоянии
        Situation curSit = state.situations.at(sitNum);

        QString rightRule = state.situations.at(sitNum).rule.right;

        for (int i = 0 ; i < rightRule.split(" ").size() ; i++){ //разделить правую часть на слова для обработки

            QString a = "", B = "", b = "";
            QStringList aa = {"$"}; //символ для свертки

            for(int j = 0 ; j < curSit.pos ; j++) a += rightRule.split(" ").value(j, "") + " "; //записать все слева
            B = rightRule.split(" ").value(curSit.pos, ""); //текущий символ
            for(int j = curSit.pos + 1 ; j < rightRule.split(" ").size() ; j++) b += rightRule.split(" ").value(j, "") + " "; //записать все справа

            aa = pickTermSimbol(b, curSit.term); //забрать терминалы для нетерминалов из первых символов

            for (auto aat : aa){
                for (auto rule : m_P) {
                    if(B != rule.left) continue; //если не то правило, то пропустить

                    Situation subSit;
                    subSit.rule = rule;
                    subSit.term = aat;
                    subSit.pos = 0;

                    if(!state.situations.contains(subSit)) {
                        state.situations << subSit;
                    }
                }
            }
        }
    }
    qApp->processEvents();
}

void MainWindow::generateStates()
{
    m_states.clear();
    m_goto.clear();

    Situation def ({m_P.at(0), 0, "$"});
    m_states.append({m_states.size(), QList<Situation>() << def}); //стартовая позиция

    //добавить в нулевое состояние все ситуации
    closure(m_states[0]);

    for (int i = 0 ; i < m_states.size() ; i++){
        generateGoTo(m_states[i]);
    }

    //вывести состояния
    QString cl;
    for(auto s : m_states){
        for(auto subSit : s.situations){
            QStringList sLeft; sLeft << "[" << subSit.rule.left << " -> ";
            QStringList sRightRule = subSit.rule.right.split(" ");
            sRightRule.size() > subSit.pos ? sRightRule.insert(subSit.pos, ".") : sRightRule.append(".");
            QStringList sRight; sRight << sRightRule << "|" << subSit.term << "]";

            cl += "S" + QString::number(s.name) + " " + sLeft.join(" ") + sRight.join(" ") + "\n"; //вывести все ситуации состояния
        }
        cl += "\n";
    }
    ui->pteOutput->setPlainText(ui->pteOutput->toPlainText() + "\n" + cl);


    QString gt;
    for (auto g : m_goto){
        gt += "goto(" + g[0] + ", " + g[1] + ") next state "+ g[2] +"\n";
    }
    ui->pteOutput->setPlainText(ui->pteOutput->toPlainText() + "\nПереходы\n" + gt);

    qApp->processEvents();
}

void MainWindow::generateGoTo(State &state)
{
    QStringList transSimb;
    for(auto subSit : state.situations){
        transSimb << subSit.rule.right.split(" ").value(subSit.pos, "").trimmed(); //найти все символы для переходов
    }
    transSimb.removeDuplicates();
    transSimb.removeOne("");

    for (auto tSimb : transSimb){ //для всех символов перехода будем генерировать новые состояния и выполнять замыкание
        State J = {m_states.size(), QList<Situation>()};

        for(auto subSit : state.situations){ //пройтись по всем ситуациям для переноса
            if (subSit.rule.right.split(" ").value(subSit.pos, "") == tSimb){

                Situation sitCopy ({subSit.rule, subSit.pos + 1, subSit.term});
                J.situations << sitCopy;
            }
        }

        //выполнить замыкание
        closure(J);

        //добавить в граф переходов
        m_goto << (QStringList() << QString::number(state.name) << tSimb);

        //проверить на наличие такого состояния
        bool isExist = false;
        for (auto S : m_states){
            if (J == S){
                isExist = true;
                m_goto.last() << QString::number(S.name); //добавить состояние куда переходить
            }
        }
        if (!isExist){ //если не существует, то добавить
            m_states.append(J); //новое состояние по переходу
            m_goto.last() << QString::number(m_states.last().name); //добавить состояние куда переходить
        }
    }
    qApp->processEvents();
}


QStringList MainWindow::pickTermSimbol(QString b, QString aa)
{
    QString temp = QString(b + " " + aa).trimmed();

    for (auto word : temp.split(" ")){
        if(m_N.contains(word)) //для нетерминала
            for (auto start : m_starts)
                if (start.first == word) return start.second; //вернуть все первые символы у нетерминала
        return {word}; //для терминала
    }
    return {"$"};
}

void MainWindow::generateReserveWords()
{
    m_reservedWords.clear();
    //обработать все терминалы и выделить ключевые слова
    for (auto w : m_E){
        if (w.contains('\\')) continue; //если начинается с \ то это ре
        if (w.contains('-')) continue; //если второй символ - то это ре
        m_reservedWords << w; //добавить к словам
    }
    m_reservedWords.removeDuplicates();
}

void MainWindow::parseInput()
{
    QString input = ui->pteInput->toPlainText() + "$";
    m_allDefs.clear();
    m_level = 0;
    m_name.clear();
    m_type.clear();
    m_args.clear();
    m_checkError.clear();
    m_process.clear();

    int step = 0;

    int absolutePos = 0;
    int charInWord = 0;
    QString nextTerminal;

    QStack<int> sState;
    sState.push(0);  //добавить сразу нулевую позицию

    QStack<QString> sText;

    auto posToStr = [&]{
        return calcPosition(absolutePos + charInWord);
    };

    ui->leResult->setText("");

    //разбор строки
    while (1) {

        //если вышли за вход
        if(absolutePos + charInWord >= input.size()){
            ui->leResult->setText("Ошибка разбора " + posToStr());
            break;
        }

        //если ошибка во встроенных действиях
        if(m_checkError.size()){
            ui->leResult->setText("Ошибка " + posToStr() + " - " +  m_checkError);
            break;
        }

        nextTerminal += input[absolutePos + charInWord];

        if ((nextTerminal[0] >= 'a' && nextTerminal[0] <='z') || nextTerminal[0] == "."){
            charInWord = checkReservedWords(nextTerminal, absolutePos, charInWord);
        }

        QString rawNext = nextTerminal; //сохранить текст слова, перед подменой для разбора

        //заменить символ на паттерн
        nextTerminal = inputToPattern(nextTerminal);
        if(nextTerminal.isEmpty()){
            ui->leResult->setText("Ошибка разбора " + posToStr());
            break;
        }

        //если это терминал, то обработать по таблице
        if(m_E.contains(nextTerminal)){ //здесь
            QStringList procRow;

            //сдвиг делать только если в следующем состоянии должн быть считан  терминал
            QString nextAction = m_data[sState.at(sState.size() - 1)][m_columns.value(nextTerminal)];

            //-----------------------------срез состояния-----------------------
            QStringList states;
            for (auto r : sState) states << QString::number(r);

            QStringList texts;
            for (auto r : sText) texts << r;

            procRow << QString::number(step++) << states.join(" ") << texts.join(" ") << nextTerminal << nextAction;

            m_process += procRow;
            //------------------------------------------------------------------


            if (nextAction.isEmpty()){
                ui->leResult->setText("Ошибка разбора " + posToStr());
                break;
            }

            if(nextAction == "HALT"){
                ui->leResult->setText("Разбор успешно завершен");
                break;
            }

            if (!nextAction.size()){
                ui->leResult->setText("Ошибка разбора " + posToStr());
                break;
            }

            if (nextAction[0] == 'S'){ //сдвиг
                sText.push(nextTerminal); //добавить в стек символов

                nextAction.remove(0, 1); //удалить первый символ
                sState.push(nextAction.toInt()); //добавить в стек состояний

                impActionE(sText.top(), rawNext); //обработать действия с терминалом

                charInWord++; //идем дальше
            }else{//свертка
                nextAction.remove(0, 1); //удалить первый символ

                //получить количество элементов из правой части правила
                int c = m_P.at(nextAction.toInt()).right.split(" ").size();

                for (int i = 0 ; i < c ; i ++){
                    sText.pop(); //вытащить элемент из текстового стека
                    sState.pop(); //вытащить элемент из состояний
                }

                sText.push(m_P.at(nextAction.toInt()).left);

                impActionN(nextAction.toInt()); //встроенное действие при выполнении свертки

                //теперь добавим переход
                QString goState;
                for (auto row : m_goto){
                    if (row[0] == QString::number(sState.last()) && row[1] == sText.last()){
                        goState = row[2];
                        break;
                    }
                }
                sState.push(goState.toInt());
                charInWord = 0;//не наращивать, если была свертка
            }
            absolutePos += charInWord;
            nextTerminal.clear();
            charInWord = 0;
        }else{
            //если нсколько символов в терминале
            charInWord++;
        }
        qApp->processEvents();
    }

    m_processModel.reset(new myTableModel(m_process, nullptr));
    ui->lvSteps->setModel(m_processModel.data());
    //шапка
    QStringList headers = {"Шаг", "Стек состояний", "Стек символов", "Следующий символ", "Следующее состояние" };
    for (int i = 0 ; i < headers.size() ; i++)
        m_processModel->setHeaderData(i, Qt::Horizontal, headers[i]);
}


QString MainWindow::inputToPattern(QString c)
{
    if (m_E.contains(c)) return c; //если терминал состоит из одного знака

    for (auto pattern : m_E){
        if(m_reservedWords.contains(pattern)) continue; //если зарезервированное слово, то проверка не требуется

        const QRegularExpression re("[" + pattern + "]+"); //найти соответствия
        auto match = re.globalMatch(c); //получить итератор
        while (match.hasNext()) {
            auto result = match.next();
            return pattern;
        }
    }
    return "";
}

int MainWindow::checkReservedWords(QString& in, int pos, int posInWord)
{
    QString input = ui->pteInput->toPlainText() + "$";

    for (auto w : m_reservedWords){ //проверить каждое слово
        bool checkWord = true; //слово подходит по умолчанию
        for (int i = 0 ; i < w.size() ; i++ ) { //проверить по буквам
            if(input[pos + i] != w[i]){ //если символ не совпадает, значит не то слово
                checkWord = false;
                break;
            }
        }

        if(checkWord){ //проверить следующий символ, потому как это можнет быть частью переменной
            QChar nx = input[pos + w.size()];
            if ((nx >= '0' && nx <= '9') || (nx >= 'A' && nx <= 'Z') || (nx >= 'a' && nx <= 'z')){
                checkWord = false;
            }
        }

        if (checkWord){
            in = w; //скопировть слово
            return w.size() - 1; //вернуть длину слова
        }
    }
    return posInWord; //иначе просто вернуть позицию
}

void MainWindow::impActionE(QString last, QString ch)
{
    if (last == "\\s") return;;

    //обработать вводимые симаволы
    last.replace("\\","\\\\");
    last.replace("[","\\[");
    last.replace("]","\\]");
    last.replace("(","\\(");
    last.replace(")","\\)");
    last.replace("{","\\{");
    last.replace("}","\\}");
    last.replace(".","\\.");
    last.replace("*","\\*");
    last.replace("&","\\&");

    for (auto rule : m_P_RAW){

        if (rule.right.contains(QRegularExpression(last + R"([\s]*<)"))){ //если содержится это слово и знак начала внедренного действия
            QString rt = rule.right;
            int startIndex = rule.right.indexOf(QRegularExpression(last));
            rt.remove(0, startIndex); //удалить все элементы слева

            const QRegularExpression re(R"(<.*?>)"); //найти соответствия
            auto match = re.globalMatch(rt); //получить итератор
            QString action;
            int lastIndex = 0;
            while (match.hasNext()) {
                auto m = match.next();

                if(lastIndex != 0 && lastIndex != m.capturedStart()) break;

                action = m.captured(); //собрать все действия для данного правила
                lastIndex = m.capturedEnd();

                if(action == "<NAMESPACE>"){
                    m_namespace << ch; //добавляем пространства имен, чтобы отслеживаьт изменения
                    m_namespace.removeDuplicates();
                }

                if(action == "<TYPE>" || action == "<NAMESPACE>"){
                    if (ch == "signed") continue; //пропускать знаковое

                    m_type += " " + ch; //собрать тип. может состоять из нескольких слов
                    m_type = m_type.simplified();

                    //упорядочить
                    QStringList keyWords = m_type.split(" ", Qt::SkipEmptyParts);
                    keyWords.sort();


                    //убрать звездочки и амперсанд в конец выражения
                    QList<QChar> simb = {"*","&"};

                    for (auto c : simb){
                        int count = keyWords.count(c);
                        keyWords.removeAll(c);
                        for (int i = 0 ; i < count ; ++i){
                            keyWords.append(c);
                        }
                    }

                    m_type = keyWords.join(" ");
                }

                if(action == "<LIT>"){
                    m_name += ch; //собираем идентификатор
                    if (m_name.contains(QRegularExpression("\\[0.*[89]")))
                        m_checkError = "неверная запись октета";
                    if (m_name.contains(QRegularExpression("\\[\\s*\\]")))
                        m_checkError = "пустой массив";
                }

                if(action == "<LEVEL_U>"){
                    m_level++;//увеличить уровень
                }

                if(action == "<LEVEL_D>"){
                    m_level--;//уменьшить уровень
                }
            }
            break;
        }
    }
}

void MainWindow::impActionN(int rn)
{

    for (auto N : m_N){
        if (m_P_RAW[rn].right.contains(QRegularExpression(N + R"([\s]*<)"))){ //если содержится это слово и знак начала внедренного действия
            QString rt = m_P_RAW[rn].right;
            int startIndex = m_P_RAW[rn].right.indexOf(QRegularExpression(N));
            rt.remove(0, startIndex); //уудалить все элементы слева

            const QRegularExpression re(R"(<.*?>)"); //найти соответствия


            auto match = re.globalMatch(rt); //получить итератор
            QString action;
            int lastIndex = 0;
            while (match.hasNext()) {
                auto m = match.next();

                if(lastIndex != 0 && lastIndex != m.capturedStart()) break;

                action = m.captured(); //собрать все действия для данного правила
                lastIndex = m.capturedEnd();

                if(action == "<ADDNAMESPACE>" || action == "<ADDFUNCNAME>"){
                    m_allDefs.append({m_level, m_type, m_name, {}, false}); //добавить элемент

                    //очишаем переменные
                    m_name.clear();
                    m_type.clear();
                    m_args.clear();
                }

                if(action == "<ADDPARAM>"){
                    QPair<QString, QString> t = {m_type, m_name};
                    m_args << t;

                    //очишаем переменные
                    m_name.clear();
                    m_type.clear();
                }

                if(action == "<CHECKARGLIST>"){
                    m_allDefs.last().args = m_args;

                    //проверим все аргументы  функции на дублирование
                    QStringList varNames;
                    for(auto v : m_args){
                        if (v.second.isEmpty()) continue;

                        if (!varNames.contains(v.second)){
                            varNames << v.second;
                        }else{
                            m_checkError = "Одинаковое имя переменной " + v.second;
                            break;
                        }
                    }

                    //очишаем переменные
                    m_name.clear();
                    m_type.clear();
                    m_args.clear();
                }

                if(action == "<CHECK>"){
                    for (int i = 0 ; i < m_allDefs.size() ; i++){
                        for (int j = 0 ; j < m_allDefs.size() ; j++){
                            if (i == j) continue;//не сравнивать с собой

                            //проверим конфликт пространства имен и функции
                            //если сравниваем объект из пространства имен с объектом не из пространства имен
                            // и если имя и уровень совпадают, то будет ошибкой
                            if (m_namespace.contains(m_allDefs[i].type) && !m_namespace.contains(m_allDefs[j].type) && m_allDefs[i].name == m_allDefs[j].name && m_allDefs[i].level == m_allDefs[j].level ) {
                                QStringList args;
                                for (auto a : m_allDefs[i].args) args << a.first;
                                m_checkError = "На одном уровне пространство имен и имя функции " + m_allDefs[i].name + ", уровень " + QString::number(m_allDefs[i].level);
                                qDebug() << m_namespace;
                                break;
                            }

                            //проверим функцию на дублирование реализации
                            //ошибкой будет одинаковые имя и типы параметров в такойже последовательности
                            if(m_namespace.contains(m_allDefs[i].type)) continue; //если пространство имен, то пропускаем проверку

                            if (m_allDefs[i] == m_allDefs[j]) {
                                QStringList args;
                                for (auto a : m_allDefs[i].args) args << a.first;
                                m_checkError = "Одинаковые реализации функции " + m_allDefs[i].name + "(" + args.join(",") + ")";
                                break;
                            }
                        }
                        if (m_checkError.size()) break; //если есть ошибка, то уже можно выходить
                    }
                }

                if(action == "<REAL>"){
                    m_allDefs.last().isProto = false;
                }
                if(action == "<PROTO>"){
                    m_allDefs.last().isProto = true;
                }
            }
        }
    }
}
