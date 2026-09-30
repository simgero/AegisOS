#!/usr/bin/env python3
"""Build test-only all-architecture Debian archives + signed metadata on aegis-build.
No guest program/package script is executed. The fresh private test key is
always destroyed. Only public signed metadata, archive bytes and a receipt leave
this directory through GitHub; these keys must never enter product trust stores.
"""
import argparse, hashlib, json, os, platform, shutil, subprocess, tempfile
from pathlib import Path


def run(args, **kw):
    return subprocess.run(args, check=True, stdin=subprocess.DEVNULL, **kw)


def main():
    parser=argparse.ArgumentParser();parser.add_argument("output",type=Path);args=parser.parse_args()
    if platform.system()!="Linux" or platform.machine()!="x86_64":
        raise SystemExit("Build the fixture only on the Linux x86-64 builder")
    out=args.output.absolute();out.mkdir(mode=0o755,exist_ok=False);os.chdir(out)
    work=out/"work";work.mkdir();repo=work/"aegis-repo";repo.mkdir()
    env=dict(os.environ,SOURCE_DATE_EPOCH="1704067200",LC_ALL="C")
    for variable in ("GH_TOKEN","GITHUB_TOKEN","CREDENTIALS_DIRECTORY"):
        env.pop(variable,None)
    records=[]
    for kind in ("app","lib"):
        for version in ("1","2"):
            name="aegis-probe-"+kind;tree=work/(name+"-"+version);(tree/"DEBIAN").mkdir(parents=True)
            control=f"Package: {name}\nVersion: {version}\nArchitecture: all\nMaintainer: AEGIS TEST ONLY <apt-fixture@invalid>\nDescription: Isolated signed-archive fixture\n"
            if kind=="app":
                control+=f"Depends: aegis-probe-lib (= {version})\n"
                binary=tree/"usr/bin/aegis-probe-app";binary.parent.mkdir(parents=True)
                binary.write_text(f"#!/bin/sh\nprintf 'app-{version}\\n'\n");binary.chmod(0o755)
            else:
                binary=tree/"usr/share/aegis-probe/library";binary.parent.mkdir(parents=True);binary.write_text(version+"\n")
            (tree/"DEBIAN/control").write_text(control)
            filename=f"{name}_{version}_all.deb";archive=repo/filename
            run(["dpkg-deb","--root-owner-group","-Zgzip","--build",str(tree),str(archive)],env=env)
            records.append(control+f"Filename: {filename}\nSize: {archive.stat().st_size}\nSHA256: {hashlib.sha256(archive.read_bytes()).hexdigest()}\n")
    index="\n".join(records).encode();(repo/"Packages").write_bytes(index)
    suffix=f"SHA256:\n {hashlib.sha256(index).hexdigest()} {len(index)} Packages\n"
    identity="Origin: AEGIS TEST ONLY\nLabel: AEGIS TEST ONLY\nSuite: aegis-test\nCodename: aegis-test\nArchitectures: arm64 all\n"
    release=(identity+"Date: Tue, 29 Sep 2026 00:00:00 UTC\nValid-Until: Tue, 29 Sep 2037 00:00:00 UTC\n"+suffix).encode()
    (repo/"Release").write_bytes(release)
    (work/"aegis-expired-Release").write_text(identity+"Date: Mon, 01 Jan 2024 00:00:00 UTC\nValid-Until: Tue, 02 Jan 2024 00:00:00 UTC\n"+suffix)
    keyhome=Path(tempfile.mkdtemp(prefix="aegis-fixture-key-",dir=out));keyhome.chmod(0o700)
    gpg=["gpg","--homedir",str(keyhome),"--batch","--pinentry-mode","loopback","--passphrase","","--faked-system-time","1704067200"]
    try:
        run(gpg+["--quick-generate-key","AEGIS TEST ONLY <apt-fixture@invalid>","ed25519","sign","0"],env=env)
        listing=subprocess.check_output(gpg+["--with-colons","--list-keys"],env=env,text=True)
        fingerprint=next(line.split(":")[9] for line in listing.splitlines() if line.startswith("fpr:"))
        with (work/"aegis-test-key.asc").open("wb") as f:run(gpg+["--armor","--export",fingerprint],stdout=f,env=env)
        for releasepath in (repo/"Release",work/"aegis-expired-Release"):
            run(gpg+["--armor","--detach-sign","--output",str(releasepath)+".gpg",str(releasepath)],env=env)
    finally:
        subprocess.run(["gpgconf","--homedir",str(keyhome),"--kill","gpg-agent"],check=False,env=env)
        shutil.rmtree(keyhome)
    files=sorted([x for x in work.rglob("*") if x.is_file() and
                  (x.parent==repo or x.parent==work)],key=lambda x:x.relative_to(work).as_posix())
    header=["// TEST ONLY. Actual .deb archives built on aegis-build; private signing key discarded.",
            "// Never install this fixture key or repository in a product configuration.",
            "#ifndef AEGIS_PACKAGE_APT_METADATA_FIXTURE_H","#define AEGIS_PACKAGE_APT_METADATA_FIXTURE_H",
            "#include <stddef.h>","struct aegis_apt_metadata_fixture { const char *name; const unsigned char *data; size_t size; };",]
    for number,path in enumerate(files):
        raw=path.read_bytes();header.append(f"static const unsigned char aegis_apt_file_{number}[] = {{")
        for offset in range(0,len(raw),24):header.append("    "+",".join(str(b) for b in raw[offset:offset+24])+",")
        header.append("};")
    header.append("static const struct aegis_apt_metadata_fixture aegis_apt_metadata[] = {")
    for number,path in enumerate(files):
        header.append(f'    {{"{path.relative_to(work).as_posix()}",aegis_apt_file_{number},sizeof(aegis_apt_file_{number})}},')
    header += ["};","#endif"]
    (out/"package_apt_metadata_fixture.h").write_text("\n".join(header)+"\n")
    receipt={"schema":1,"scope":"TEST_ONLY_NOT_PRODUCT_TRUST","fingerprint":fingerprint,
             "index_sha256":hashlib.sha256(index).hexdigest(),"release_sha256":hashlib.sha256(release).hexdigest(),
             "valid_until_unix":2137795200,"archives":{p.name:{"bytes":p.stat().st_size,"sha256":hashlib.sha256(p.read_bytes()).hexdigest()} for p in files if p.suffix==".deb"}}
    (out/"fixture.json").write_text(json.dumps(receipt,indent=2)+"\n")
    names=("package_apt_metadata_fixture.h","fixture.json")
    (out/"SHA256SUMS").write_text("".join(hashlib.sha256((out/name).read_bytes()).hexdigest()+"  "+name+"\n" for name in names))
    shutil.rmtree(work)
    print("TEST_FIXTURE_BUILT_PRIVATE_KEY_DISCARDED",out)

if __name__=="__main__":main()
