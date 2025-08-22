# Demonstrator Threat Model

The AttackDefense Framework (ADF) is used to model threats to a device in the following phases:

## 1. System and Attacker Models

The demonstrator platform is a complex system that uses BLE for wireless communication between nodes and secure channels to exchange secret data between the application and Secure Element.

This introduces potential threats at the protocol and implementation levels for both wireless and wired communications, physical threats for secure element(s), and software threats for all computational elements.

Based on our assumptions, we set **Confidentiality**, **Authenticity**, and **Integrity** requirements for the BLE and wired communications, the physical implementation of secure element(s), and the software running on all system parts.

To demonstrate the wide range of threats covered by the compiled ADF database, we consider two attackers with broad capabilities, while narrower attacker models may always be defined to limit the scope:

  1. The attacker with MitM capabilities, considering wireless communication only. The attacker can eavesdrop on, intercept, and modify legitimate BLE communications. We assume a secure BLE stack implementation, and we focus only on protocol-level threats.
  2. The attacker with physical access to the system may connect to on-board buses, intercept and modify bus communications, or even mount physical attacks on secure elements.


## 2. Threats Enumeration

In this phase, the attack surface is identified, leading to a list of potential vulnerabilities and threats.
For this purpose, our [extended ADF](https://github.com/tropicsquare/orshin-adf/) is used to [visualize](https://tropicsquare.github.io/orshin-adf/) potential vulnerabilities related to the system model in a tree view and as a *threat catalogue*.

The extended ADF approach enables simpler device modeling by following [MITRE EMB3D](https://emb3d.mitre.org) device properties while allowing extension by ADF, providing a more detailed attack surface view as a result.

By combining ADF and MITRE EMB3D in this way, it is possible to achieve more efficient and detailed modeling — particularly in areas where [MITRE EMB3D](https://emb3d.mitre.org) lacks in-depth coverage.
For constrained devices, we defined the AD databases based on ORSHIN expert catalogs available as a [generic catalog](https://github.com/tropicsquare/orshin-adf/catalog-mitre).

The source model files used to generate the [visualization](https://tropicsquare.github.io/orshin-adf/):

  * [model_physical.yaml](https://github.com/tropicsquare/orshin-adf/tree/main/visualization/_data/model_physical.yaml)
  * [model_software.yaml](https://github.com/tropicsquare/orshin-adf/tree/main/visualization/_data/model_software.yaml)
  * [model_bt.yaml](https://github.com/tropicsquare/orshin-adf/tree/main/visualization/_data/model_bt.yaml)

## 3. Threats Ranking

For threat scoring, the programmatic approach presented in the original [ADF Usage Example to TM a Cryptowallet blog post](https://github.com/tropicsquare/orshin-adf/blob/main/blogpost.md) can be employed.
The disadvantage of the programmatic approach is that the risk assessment depends strongly on the quality and size of the underlying AD database.

The [extended](https://tropicsquare.github.io/orshin-adf/) approach brings the standard tabular view of the *threats catalogue*, helping in semi-manual risk assessment, as it connects the ADF-based threat model with [MITRE EMB3D](https://emb3d.mitre.org) and [MITRE CVE](https://www.cve.org/) databases, supporting the view of the *Risk score*, typically the *CVSS value*. The extraction of missing score values is supported by linkage with [MITRE EMB3D](https://emb3d.mitre.org) and [MITRE CVE](https://www.cve.org/) databases, providing a wider range of data supporting *Risk score* determination.

Both approaches can also be used to enumerate available threats and defenses, while the difference is in:
  * pure textual vs. webpage-based tabular presentation conventional in the field of risk analysis,
  * connection with MITRE databases in the latter case provides easy adoption and a wider range of covered attack surfaces, and might support the underlying database’s semi-automatic maintenance

Note that both approaches use the same underlying database and thus can be combined.

## 4. Defense Strategy

A detailed defense strategy is not relevant for the demonstrator; however:
  - [BlueBrothers-protocols](https://github.com/sacca97/bb-protocols) addresses the vast majority of the enumerated Bluetooth protocol threats
  - [New Secure Channel Protocol 03](https://github.com/securitypattern/orshin-STM32-client-scp03-nscp) addresses the vast majority of the enumerated SCP03 protocol threats
