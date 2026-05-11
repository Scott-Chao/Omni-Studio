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
    void setTagNames(const QStringList &names);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void updateCompleter();

private:
    void insertCompletion(const QString &completion);
    void insertTagCompletion(const QString &completion);

    QCompleter *m_completer;
    QStringListModel *m_model;
    QStringListModel *m_tagModel;
    bool m_inTagMode = false;
};

#endif // WIKILINKTEXTEDIT_H
