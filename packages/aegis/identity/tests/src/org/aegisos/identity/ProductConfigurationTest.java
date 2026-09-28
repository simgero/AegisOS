package org.aegisos.identity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.os.UserManager;
import android.os.Process;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import java.util.Arrays;
import java.util.Locale;

import org.junit.Test;
import org.junit.runner.RunWith;

/** Reads the installed product's effective resources; creates no users and changes no state. */
@RunWith(AndroidJUnit4.class)
public final class ProductConfigurationTest {
    @Test public void installedRuntimeResourceNamesResolveToTheirReservedIds() throws Exception {
        // Check actual installed partition registries through bionic. A generic
        // OEM fallback must not make missing generated names look successful.
        for (int inside = 0; inside < 1000; inside++) {
            checkResourceName(String.format(Locale.ROOT, "vendor_aegis_linux_%04d", inside),
                    5000 + inside);
        }
        checkResourceName("system_ext_aegis_runtime_user", 7500);
        checkResourceName("system_ext_aegis_runtime_nobody", 7501);
    }

    private static void checkResourceName(String name, int appId) throws Exception {
        // Os.getpwnam/StructPasswd are not in the stable libcore platform API.
        // Process delegates these named lookups to bionic. The native registry
        // test additionally checks the canonical reverse passwd/group names.
        assertEquals("Missing generated user resource " + name, appId, Process.getUidForName(name));
        assertEquals("Missing generated group resource " + name, appId, Process.getGidForName(name));
    }

    @Test public void frameworkStartsExactlyOneAegisIdentityService() throws Exception {
        Context target = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Resources framework = target.createPackageContext("android", 0).getResources();
        int id = framework.getIdentifier("config_deviceSpecificSystemServices", "array", "android");
        assertTrue("Framework service-list resource is missing", id != 0);
        long count = Arrays.stream(framework.getStringArray(id))
                .filter("org.aegisos.identity.AegisIdentityService"::equals).count();
        assertEquals("AEGIS boot service must be present exactly once", 1L, count);
    }

    @Test public void productSupportsSystemAndThreePersonalUsers() {
        // Two persistent people plus the later new-user/shared-package test need four slots.
        assertTrue("Inherited multi-user configuration was lost",
                UserManager.getMaxSupportedUsers() >= 4);
        // The initial logout coordinator hands foreground control back to full system user 0.
        assertFalse("This product's logout handover requires a full system user",
                UserManager.isHeadlessSystemUserMode());
    }

    @Test public void unsupportedRadiosAreNotAdvertised() {
        PackageManager pm = InstrumentationRegistry.getInstrumentation()
                .getTargetContext().getPackageManager();
        // Inspect the installed system after all inherited/APEX permissions have
        // been merged, including any new subfeatures introduced by an update.
        for (FeatureInfo feature : pm.getSystemAvailableFeatures()) {
            String name = feature.name;
            if (name == null) continue; // The OpenGL ES version entry has no name.
            boolean unsupported = name.equals("android.hardware.nfc")
                    || name.startsWith("android.hardware.nfc.")
                    || name.equals("android.software.nfc.beam")
                    || name.equals("android.hardware.bluetooth")
                    || name.startsWith("android.hardware.bluetooth.")
                    || name.equals("android.hardware.bluetooth_le")
                    || name.equals("android.hardware.uwb")
                    || name.equals("android.hardware.thread_network")
                    || name.equals("android.hardware.telephony")
                    || name.startsWith("android.hardware.telephony.");
            assertFalse("Standalone QEMU has no controller for " + name, unsupported);
        }
    }

    @Test public void productDoesNotForceCellularInitializationWithoutAModem() throws Exception {
        Context target = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Resources framework = target.createPackageContext("android", 0).getResources();
        for (String name : new String[] {"config_force_phone_globals_creation",
                "config_voice_capable", "config_sms_capable"}) {
            int id = framework.getIdentifier(name, "bool", "android");
            assertTrue("Missing effective telephony configuration " + name, id != 0);
            assertFalse("Standalone QEMU has no modem: " + name, framework.getBoolean(id));
        }
    }
}
