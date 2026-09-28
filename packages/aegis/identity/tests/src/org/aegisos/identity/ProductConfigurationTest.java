package org.aegisos.identity;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.Resources;
import android.os.UserManager;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import java.util.Arrays;

import org.junit.Test;
import org.junit.runner.RunWith;

/** Reads the installed product's effective resources; creates no users and changes no state. */
@RunWith(AndroidJUnit4.class)
public final class ProductConfigurationTest {
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
}
