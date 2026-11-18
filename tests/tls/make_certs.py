#!/usr/bin/env python3
"""Spec 019 (research R11): the test certificates of tests/tls/certs/, made once and committed.

  make_certs.py      rewrites tests/tls/certs/ (new keys each time; the structure is fixed)

Uses the Python `cryptography` package (a test-only tool). Each server certificate NAME has NAME.pem
(the chain the server sends: leaf, then intermediates) and NAME.key. Authorities: test-ca.pem (the one
the cases give with --ca-file), rogue-ca.pem (never given). The intermediate test-int signs leaves.

  a.test          RSA-2048 leaf for a.test and 127.0.0.1, valid 2020-2120, sent with the intermediate
  a.test-ecdsa    the same with a P-256 key (for the ECDHE-ECDSA suites)
  b.test          a leaf for b.test and 127.0.0.2 (the second server of ca_file_two, via rogue-ca)
  expired         valid 2020-01-01 to 2021-01-01
  future          valid 2100-01-01 to 2200-01-01
  other-name      valid, but for b.test only
  self-signed     its own issuer, for 127.0.0.1
  rogue           signed by rogue-ca
  no-intermediate the a.test leaf alone (the server leaves out the intermediate)
"""
import datetime
import ipaddress
import os

from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, rsa
from cryptography.x509.oid import NameOID

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "certs")
D = datetime.datetime


def name(cn):
    return x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, cn)])


def key(kind="ec"):
    return ec.generate_private_key(ec.SECP256R1()) if kind == "ec" else rsa.generate_private_key(65537, 2048)


def cert(subject, skey, issuer, ikey, start, end, ca=False, sans=()):
    b = (x509.CertificateBuilder().subject_name(name(subject)).issuer_name(name(issuer))
         .public_key(skey.public_key()).serial_number(x509.random_serial_number())
         .not_valid_before(start).not_valid_after(end)
         .add_extension(x509.BasicConstraints(ca=ca, path_length=None), critical=True))
    if ca:
        b = b.add_extension(x509.KeyUsage(False, False, False, False, False, True, True, False, False), critical=True)
    if sans:
        alt = [x509.IPAddress(ipaddress.ip_address(s)) if s[0].isdigit() else x509.DNSName(s) for s in sans]
        b = b.add_extension(x509.SubjectAlternativeName(alt), critical=False)
    return b.sign(ikey, hashes.SHA256())


def pem(c):
    return c.public_bytes(serialization.Encoding.PEM)


def write(n, chain, k=None):
    open(os.path.join(OUT, n + ".pem"), "wb").write(b"".join(pem(c) for c in chain))
    if k is not None:
        open(os.path.join(OUT, n + ".key"), "wb").write(k.private_bytes(
            serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))


def main():
    os.makedirs(OUT, exist_ok=True)
    long = (D(2020, 1, 1), D(2120, 1, 1))
    ca_k = key()
    ca = cert("vo test CA", ca_k, "vo test CA", ca_k, *long, ca=True)
    write("test-ca", [ca])
    int_k = key()
    inter = cert("vo test intermediate", int_k, "vo test CA", ca_k, *long, ca=True)
    rogue_k = key()
    rogue_ca = cert("rogue CA", rogue_k, "rogue CA", rogue_k, *long, ca=True)
    write("rogue-ca", [rogue_ca])
    leaf_k = key("rsa")
    leaf = cert("a.test", leaf_k, "vo test intermediate", int_k, *long, sans=("a.test", "127.0.0.1"))
    write("a.test", [leaf, inter], leaf_k)
    write("no-intermediate", [leaf], leaf_k)
    ek = key()
    write("a.test-ecdsa", [cert("a.test", ek, "vo test intermediate", int_k, *long, sans=("a.test", "127.0.0.1")), inter], ek)
    bk = key()
    write("b.test", [cert("b.test", bk, "rogue CA", rogue_k, *long, sans=("b.test", "127.0.0.1"))], bk)
    k = key()
    write("expired", [cert("a.test", k, "vo test intermediate", int_k, D(2020, 1, 1), D(2021, 1, 1),
                           sans=("a.test", "127.0.0.1")), inter], k)
    k = key()
    write("future", [cert("a.test", k, "vo test intermediate", int_k, D(2100, 1, 1), D(2200, 1, 1),
                          sans=("a.test", "127.0.0.1")), inter], k)
    k = key()
    write("other-name", [cert("b.test", k, "vo test intermediate", int_k, *long, sans=("b.test",)), inter], k)
    k = key()
    write("self-signed", [cert("127.0.0.1", k, "127.0.0.1", k, *long, sans=("127.0.0.1",))], k)
    k = key()
    write("rogue", [cert("a.test", k, "rogue CA", rogue_k, *long, sans=("a.test", "127.0.0.1"))], k)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
