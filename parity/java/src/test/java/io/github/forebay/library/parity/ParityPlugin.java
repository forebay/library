package io.github.forebay.library.parity;

import io.github.intisy.ai.engine.ActivationUnit;
import io.github.intisy.ai.engine.DeclarationKind;
import io.github.intisy.ai.engine.ManifestFacts;
import io.github.intisy.ai.engine.PluginSession;

import java.util.Collections;

/** A plugin that requires the apps service and holds what its session handed it during activation. */
final class ParityPlugin {

    static final String ID = "apps-parity";

    private AppsService apps;

    static ManifestFacts facts() {
        return new ManifestFacts(ID, 1, null, null, null)
                .withServices(Collections.singletonList(AppsService.APPS.id()), null);
    }

    static ActivationUnit unit() {
        ActivationUnit.Need apps = new ActivationUnit.Need(DeclarationKind.SERVICE, AppsService.APPS.id(), false, false);
        return new ActivationUnit(ID, Collections.<ActivationUnit.Offer>emptyList(), Collections.singletonList(apps));
    }

    void activate(PluginSession session) {
        apps = (AppsService) session.getServices().get(AppsService.APPS.id());
    }

    AppsService apps() {
        if (apps == null) {
            throw new IllegalStateException("the parity plugin was never activated, so it holds no apps service");
        }
        return apps;
    }
}
