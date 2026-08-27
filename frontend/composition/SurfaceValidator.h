#ifndef A2UI_SURFACE_VALIDATOR_H
#define A2UI_SURFACE_VALIDATOR_H

#include <QByteArray>
#include <QMap>
#include <QStringList>

class QJsonDocument;

namespace a2ui {

struct SurfaceValidationResult
{
    SurfaceValidationResult();

    bool isValid;
    QStringList diagnostics;
};

class SurfaceValidator
{
public:
    struct Limits {
        Limits();

        int maximumNodes;
        int maximumDepth;
    };

    explicit SurfaceValidator(const QMap<QString, bool> &catalogMultiplicity = demoCatalog(),
                              const Limits &limits = Limits());

    SurfaceValidationResult validate(const QByteArray &json) const;
    SurfaceValidationResult validate(const QJsonDocument &document) const;

    static QMap<QString, bool> demoCatalog();

private:
    QMap<QString, bool> catalogMultiplicity_;
    Limits limits_;
};

} // namespace a2ui

#endif
