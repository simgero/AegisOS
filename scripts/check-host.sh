#!/usr/bin/env bash
# Lesende Entwicklungs-Vorpruefung; keine Installation oder Systemaenderung.
set -euo pipefail
export LC_ALL=C

if [[ $# -gt 1 ]]; then
    printf 'Verwendung: bash scripts/check-host.sh [workspace-verzeichnis]\n' >&2
    exit 2
fi
if [[ $(uname -s) != Linux ]]; then
    printf 'Bitte auf dem Linux-Entwicklungsrechner ausfuehren, nicht auf dem Mac.\n' >&2
    exit 2
fi
workspace=${1:-$PWD}
if [[ ! -d "$workspace" ]]; then
    printf 'Workspace ist kein vorhandenes Verzeichnis: %s\n' "$workspace" >&2
    exit 2
fi

printf '=== AegisOS: Entwicklungsrechner ===\n'
printf 'Architektur: %s\n' "$(uname -m)"
printf 'Kernel: %s\n' "$(uname -r)"
if command -v nproc >/dev/null 2>&1; then
    printf 'Verfuegbare CPUs: %s\n' "$(nproc)"
fi
if [[ -r /proc/meminfo ]]; then
    awk '/^MemTotal:/ {printf "RAM gesamt: %.1f GiB\n", $2/1048576}
         /^MemAvailable:/ {printf "RAM verfuegbar: %.1f GiB\n", $2/1048576}
         /^SwapTotal:/ {printf "Swap gesamt: %.1f GiB\n", $2/1048576}' /proc/meminfo
fi
printf '\nDateisystem am Workspace:\n'
df -h -- "$workspace"

printf '\n=== Hinweise fuer Android-Virtualisierung ===\n'
if command -v systemd-detect-virt >/dev/null 2>&1; then
    virtualization=$(systemd-detect-virt 2>/dev/null) || virtualization=unbekannt
    printf 'Erkannte Umgebung: %s\n' "$virtualization"
fi
case "$(uname -m)" in
    x86_64|i?86)
        if [[ ! -r /proc/cpuinfo ]]; then
            printf 'CPU-Virtualisierungsflags: nicht lesbar.\n'
        elif grep -Eq '(^|[[:space:]])(vmx|svm)([[:space:]]|$)' /proc/cpuinfo; then
            printf 'CPU-Virtualisierungsflags: vorhanden.\n'
        else
            printf 'WARNUNG: Keine vmx/svm-Flags sichtbar; bei einer VM Nested Virtualization pruefen.\n'
        fi
        ;;
esac
if [[ -c /dev/kvm ]]; then
    if [[ -r /dev/kvm && -w /dev/kvm ]]; then
        printf '/dev/kvm: vorhanden und fuer diesen Benutzer les-/schreibbar.\n'
    else
        printf 'WARNUNG: /dev/kvm vorhanden, Benutzerzugriff fehlt.\n'
    fi
else
    printf 'WARNUNG: /dev/kvm fehlt. KVM/Cuttlefish noch nicht als nutzbar bestaetigt.\n'
fi
printf '\nDies ist nur eine Bestandsaufnahme, kein VM-Start- oder Sicherheitsnachweis.\n'
printf 'Kein AOSP-Download und keine Konfiguration wurden ausgefuehrt.\n'
