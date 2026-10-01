package org.aegisos.identity;

/** CLI spelling only. The existing service still authenticates and validates every request. */
final class PackageCommand {
    private PackageCommand() { }

    static String[] fromLinux(String[] args) {
        if (args.length < 3 || !"linux".equals(args[0]) || !"package".equals(args[1])) {
            throw new IllegalArgumentException("Expected linux package action");
        }
        String action = args[2];
        if ("status".equals(action) || "approve".equals(action) || "cancel".equals(action)) {
            if (args.length != 3) throw new IllegalArgumentException("Unexpected job arguments");
            return new String[]{"package", action};
        }
        if (!"install".equals(action) && !"remove".equals(action) && !"update".equals(action)) {
            throw new IllegalArgumentException("Unknown package action");
        }
        String scope = null, operand = null;
        for (int i = 3; i < args.length; i++) {
            if ("--scope".equals(args[i])) {
                if (scope != null || ++i == args.length
                        || !("user".equals(args[i]) || "all".equals(args[i]))) {
                    throw new IllegalArgumentException("Choose exactly one scope: user or all");
                }
                scope = args[i];
            } else {
                if (args[i].isEmpty() || args[i].startsWith("-") || operand != null) {
                    throw new IllegalArgumentException("Unexpected package argument");
                }
                operand = args[i];
            }
        }
        if (scope == null) throw new IllegalArgumentException("Explicit package scope required");
        if ("update".equals(action)) {
            if (operand != null) throw new IllegalArgumentException("Update takes no package argument");
            return new String[]{"package", action, "--" + scope};
        }
        if (operand == null) throw new IllegalArgumentException("Package required");
        int equal = operand.indexOf('=');
        if (equal >= 0) {
            if (!"install".equals(action) || equal == 0 || equal == operand.length() - 1
                    || operand.indexOf('=', equal + 1) >= 0) {
                throw new IllegalArgumentException("Expected NAME or NAME=VERSION for install");
            }
            return new String[]{"package", action, "--" + scope,
                    operand.substring(0, equal), operand.substring(equal + 1)};
        }
        return new String[]{"package", action, "--" + scope, operand};
    }
}
