#ifndef MENTIONSMODEL_H
#define MENTIONSMODEL_H


#include <QAbstractListModel>
#include <QVariantList>
#include "twitterapi.h"

class MentionsModel : public QAbstractListModel
{
    Q_OBJECT
public:
    MentionsModel(TwitterApi *twitterApi);

    virtual int rowCount(const QModelIndex&) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

    Q_INVOKABLE void update();

signals:
    void updateMentionsFinished();
    void updateMentionsError(const QString &errorMessage);

public slots:
    void handleUpdateMentionsSuccessful(const QVariantList &result);
    void handleUpdateMentionsError(const QString &errorMessage);

private:
    QVariantList mentions;
    TwitterApi *twitterApi;
};

#endif // MENTIONSMODEL_H
