#ifndef PLANAVIONWIDGET_H
#define PLANAVIONWIDGET_H

#include <QList>
#include <QMap>
#include <QRect>
#include <QSet>
#include <QString>
#include <QWidget>

// Plan interactif de cabine d'avion dessiné avec QPainter (toilettes, classes, sièges libres/occupés).
class PlanAvionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlanAvionWidget(QWidget *parent = nullptr);

    void configurerCabine(int nbPassagersRequis, const QSet<QString> &siegesOccupes);
    void definirMonnaie(const QString &codeMonnaie);
    QStringList siegesSelectionnes() const { return m_siegesChoisis; }
    double calculerSupplementTotal() const;

    static bool estSiegeClasseA(const QString &codeSiege);
    static QString formaterLibelleSiege(const QString &codeSiege,
                                        const QString &codeMonnaie = QStringLiteral("EUR"));

signals:
    void selectionModifiee();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    int m_nbPassagers = 1;
    QString m_codeMonnaie = QStringLiteral("EUR");
    QSet<QString> m_occupes;
    QStringList m_siegesChoisis;
    QMap<QString, QRect> m_zonesSieges;
};

#endif // PLANAVIONWIDGET_H
