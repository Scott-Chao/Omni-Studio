#ifndef WIKILINKTEXTEDIT_H
#define WIKILINKTEXTEDIT_H

#include <QTextEdit>
#include <QCompleter>
#include <QStringListModel>

class WikiLinkTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit WikiLinkTextEdit(QWidget *parent = nullptr);

    void setFileNames(const QStringList &names);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateCompleter();

private:
    void insertCompletion(const QString &completion);

    QCompleter *m_completer;
    QStringListModel *m_model;
};

#endif // WIKILINKTEXTEDIT_H
