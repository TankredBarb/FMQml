#pragma once

#include "../../core/OpenWithService.h"

class LinuxOpenWithBackend final : public OpenWithBackend {
public:
    QList<OpenWithCandidate> enumerateCandidates(const OpenWithTarget &target) const override;
    OpenWithResult launch(const QList<OpenWithTarget> &targets, const OpenWithCandidate &candidate) const override;
};
