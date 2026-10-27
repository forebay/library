package io.github.forebay.library.parity;

import io.github.intisy.ai.engine.Activation;
import io.github.intisy.ai.engine.ActivationPlan;
import io.github.intisy.ai.engine.DeclarationKind;
import io.github.intisy.ai.engine.EventBus;
import io.github.intisy.ai.engine.PluginException;
import io.github.intisy.ai.engine.PluginHost;
import io.github.intisy.ai.engine.Scheduler;
import io.github.intisy.ai.seam.jvm.library.LibraryBridge;
import io.github.intisy.ai.seam.jvm.library.LibraryContext;
import io.github.intisy.ai.seam.jvm.library.LibraryPin;
import io.github.intisy.ai.seam.jvm.library.LibraryServices;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * A JVM host whose only plugin requires the apps service, handing the library one case's facts.
 *
 * @implNote weivel/java offers no JVM {@code PluginContext}, so the plugin is planned the way the
 * engine plans any unit, refused when its mandatory need for {@code apps} is unmet, and activated
 * against the {@code PluginSession} the host opens for it.
 */
final class ParityHost implements AutoCloseable {

    private static final Map<String, Class<?>> INTERFACES = Collections.<String, Class<?>>singletonMap(AppsService.APPS.id(), AppsService.class);

    private final List<LibraryBridge> opened;
    private final AppsService apps;

    private ParityHost(List<LibraryBridge> opened, AppsService apps) {
        this.opened = opened;
        this.apps = apps;
    }

    static ParityHost start(StagedCase staged, List<LibraryPin> pins, Path libraryCache) {
        final LibraryContext context = new LibraryContext(staged.environment(), staged.home(), staged.arguments());
        PluginHost host = new PluginHost("parity", 1, null);
        LibraryServices services = new LibraryServices(host, () -> context);
        List<LibraryBridge> opened = new ArrayList<LibraryBridge>();
        List<String> refusals = new ArrayList<String>();
        for (LibraryPin pin : pins) {
            try {
                opened.add(services.load(pin, libraryCache, INTERFACES));
            } catch (PluginException refused) {
                refusals.add(describe(refused));
            }
        }
        List<String> provided = new ArrayList<String>();
        for (String serviceId : INTERFACES.keySet()) {
            if (host.service(serviceId) != null) {
                provided.add(serviceId);
            }
        }
        ActivationPlan plan = Activation.order(Collections.singletonList(ParityPlugin.unit()), DeclarationKind.SERVICE, provided);
        if (!plan.getOrder().contains(ParityPlugin.ID)) {
            for (PluginException refused : plan.getRefusals()) {
                refusals.add(describe(refused));
            }
            closeAll(opened);
            throw new IllegalStateException("the parity plugin did not activate for case " + staged.name() + ": " + refusals);
        }
        ParityPlugin plugin = new ParityPlugin();
        plugin.activate(host.sessionFor(ParityPlugin.facts(), new SilentBus()));
        return new ParityHost(opened, plugin.apps());
    }

    AppsService apps() {
        return apps;
    }

    @Override
    public void close() {
        closeAll(opened);
    }

    private static void closeAll(List<LibraryBridge> bridges) {
        for (LibraryBridge bridge : bridges) {
            bridge.close();
        }
    }

    private static String describe(PluginException refused) {
        return refused.getPluginId() + ": " + refused.getDetail();
    }

    private static final class SilentBus implements EventBus {
        @Override
        public void publish(String topic, Object payload) {
        }

        @Override
        public Scheduler.Cancellable subscribe(String topic, Listener listener) {
            return new Scheduler.Cancellable() {
                @Override
                public void cancel() {
                }
            };
        }
    }
}
