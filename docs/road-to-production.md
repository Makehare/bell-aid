# Bell Aid: road to production

**From the delivered design files to a product you can sell in the EU (Germany first)**

Version 1.0, 1 November 2026, Makehare

*General information, not legal advice.*

---

## 0. Summary

Bell Aid is a doorbell signaller. A sender near the doorbell listens with a MEMS microphone, recognises the trained doorbell sound and sends an ESP-NOW radio message (2.4 GHz). Receivers light a 12-LED ring. Every unit runs from a 5 V USB-C wall plug. The delivered design (board DBN-MAIN rev A, firmware, 3D-printed housing) is a prototype-stage design. It is not yet a series product.

Our recommendations:

1. **Sell it as a doorbell signalling device for the home, not as a medical device.** Your claims decide which law applies (section 1).
2. **Keep the pre-certified Espressif module (ESP32-S3-MINI-1).** The module's radio test reports can feed into your own assessment. The product as a whole still needs its own CE assessment (EMC, radio, safety, RF exposure) under the Radio Equipment Directive (section 2).
3. **Make one design revision (rev B) before you request quotes.** It replaces the discontinued LED, splits the board into sender and receiver variants, removes the hand work and prepares the housing for series production (section 3).
4. **Start with printed or cast housings and one EMS partner.** Move to injection moulding once demand is proven. Tooling is the largest single cost (section 6).
5. **Register before the first sale:** WEEE (stiftung ear) and packaging (LUCID). Then prepare the technical file and the EU declaration of conformity, and plan for the Cyber Resilience Act, which fully applies from 11 December 2027 (sections 2 and 7).

Figures marked *(estimate)* are planning ranges from typical market prices, not quotes. Get at least three quotes for each.

---

## 1. Accessibility aid, not a medical device

The EU Medical Device Regulation (MDR) defines a medical device by its intended medical purpose. One of these purposes is the "alleviation of, or compensation for, an injury or disability".[7] The **intended purpose** is what the manufacturer states "on the label, in the instructions for use or in promotional or sales materials or statements".[7] So the classification follows from your wording, not only from the hardware.

Why it matters:

- **As a consumer radio product** Bell Aid needs a self-declared CE assessment under the Radio Equipment Directive (RED), done with harmonised standards and an accredited test lab (section 2).
- **As a medical device** it would also need the MDR route: a quality management system, a clinical evaluation, post-market surveillance and registration duties. That costs far more time and money and brings no benefit for a doorbell light.

Guidance points the same way. The UK regulator (MHRA) lists "personal alarm systems and home alarm systems" as "aids for daily living" that "are not medical devices", because they "may be used 'by all'".[8] This is UK guidance, so treat it as an indication for the EU, not a ruling.

Market practice in Germany agrees. The statutory health insurers list doorbell signal systems for deaf and hard-of-hearing people as "Signalanlagen" in their aids register (product group 13, subgroup 13.99.04).[34] An established signal system listed there, a radio doorbell transmitter, is CE-marked under the RED, RoHS and WEEE rules, not as a medical device.[35] This is one manufacturer's own classification, not an authority ruling, but it shows the route is used in practice for products aimed at people with hearing loss.

**Wording to use:** "visual doorbell signal", "see when someone rings", "useful for people with hearing loss, in loud rooms or with headphones on".
**Wording to avoid:** "treats", "compensates for hearing loss", "medical", "therapy", "for patients", and any claim of clinical benefit.

---

## 2. Certification and compliance (EU, Germany first)

### 2.1 Who is responsible

Under the RED, the **manufacturer** is whoever "manufactures radio equipment or has radio equipment designed or manufactured, and markets that equipment under his name or trade mark".[1] If you sell Bell Aid under your brand, **you are the manufacturer**, even if a contract manufacturer builds it. Radio products also need an economic operator established in the EU (manufacturer, importer, authorised representative or fulfilment service provider) before they can be placed on the EU market.[10] In Germany the Bundesnetzagentur (BNetzA) carries out market surveillance for radio and EMC equipment. It checks products sold online and in shops, and it reports missing CE markings and missing operator details as typical defects.[22]

### 2.2 Radio Equipment Directive 2014/53/EU (RED)

The RED has three essential requirements that apply to Bell Aid:[1]

| RED article | What it covers | Standard usually applied |
|---|---|---|
| 3(1)(a) Safety and health | Electrical safety, fire, heat. The LVD safety objectives apply "with no voltage limit".[1] | EN IEC 62368-1 (audio/video and ICT equipment safety)[4] |
| 3(1)(a) Health: RF exposure | Exposure to the radio field | EN 62479 (low-power equipment) or EN 62311[4] |
| 3(1)(b) EMC | Emissions and immunity | ETSI EN 301 489-1 and EN 301 489-17 (wideband data equipment)[3] |
| 3(2) Radio spectrum | Efficient use of the 2.4 GHz band | ETSI EN 300 328 (2.4 GHz wideband data equipment)[3] |

Only the ETSI standards in this table are cited in the Official Journal under the RED and so give a presumption of conformity: EN 300 328 and EN 301 489-17, which applies EN 301 489-1 by reference. EN IEC 62368-1, EN 62479 and EN 62311 are not cited under the RED. They are cited under the Low Voltage Directive only, and for EN 62368-1 only the old 2014 edition; the current edition is EN IEC 62368-1:2024+A11:2024.[3][4] This is no obstacle: for the safety and EMC requirements the manufacturer may always use internal production control (Annex II) and show conformity by other means, in practice a lab test to the current EN IEC 62368-1 edition.[2] Ask the lab which edition it tests to and record the reasoning in the technical file.

If the manufacturer applies harmonised standards cited in the Official Journal, the RED allows **internal production control** (Annex II) for the radio-spectrum part as well. That means self-declaration with no notified body.[1] In practice you have an accredited lab test the finished product against the standards above, then sign the declaration yourself.

The 2.4 GHz band (2400–2483.5 MHz) is under a general authorisation in Germany (BNetzA Vfg. 128/2023), so users need no individual licence.[21]

### 2.3 Pre-certified module vs. chip-down

The design uses the **ESP32-S3-MINI-1-N8 module** with an integrated PCB antenna. Espressif publishes CE and FCC documents for this module.[23][24]

- **What the module gives you:** a tested radio and antenna. Its EN 300 328 radio report can usually be reused if you keep the antenna area, the layout rules and the radio settings within the module's tested conditions. That makes radio testing cheaper and less risky.
- **What it does not give you:** your own CE marking. Once a radio is built in "in a fixed and permanent way", the combination "is deemed to be a single product", and you as the manufacturer assess the whole product.[2] EMC, safety and RF exposure are always tested on Bell Aid itself.
- **Chip-down (bare ESP32-S3 + own antenna):** saves about 1–2 € per unit *(estimate)* but needs RF layout and antenna tuning plus full radio testing. It only pays off at high volume. **Recommendation: keep the module.**
- **Radio settings to freeze before testing:** channel, transmit power and whether the optional ESP-NOW "Long Range" mode is shipped. The lab must test the modes you actually ship.

### 2.4 Cybersecurity: RED Delegated Regulation 2022/30 and Cyber Resilience Act

- **RED Art. 3(3)(d–f), Delegated Regulation (EU) 2022/30.** This applies to radio equipment "that can communicate itself over the internet, whether it communicates directly or via any other equipment".[5] The Commission's RED guide treats equipment that "does not operate a protocol … that allows communication over the internet" as not internet-connected. The manufacturer's risk assessment must still confirm this.[2] Bell Aid's current firmware uses only ESP-NOW and has no IP stack and no OTA update. Our reading is that it is **not internet-connected**. Document that reasoning in the technical file. If you later add Wi-Fi, an app or OTA updates before 11 December 2027, EN 18031-1 applies, and its listed restrictions apply too (for example, the user must not be allowed to skip setting a password).[3] Delegated Regulation (EU) 2026/339 repeals Delegated Regulation 2022/30 with effect from 11 December 2027; from that date the CRA covers these cybersecurity requirements instead.[32]
- **Cyber Resilience Act, Regulation (EU) 2024/2847 (CRA).** The CRA covers products "the intended purpose or reasonably foreseeable use of which includes a direct or indirect logical or physical data connection to a device or network".[6] Bell Aid's radio link is such a connection (our reading). The CRA applies from **11 December 2027**. Its vulnerability and incident reporting duties (Art. 14) apply from 11 September 2026, including to products placed on the market earlier.[6] Bell Aid falls in the CRA's default category, so self-assessment (internal control) is allowed: the closest listed category, "smart home products with security functionalities", covers products that protect the physical security of consumers and can be controlled remotely, which a doorbell light does not do (our reading).[33] Each unit placed on the market from 11 December 2027 must comply. If you will still be selling after that date, plan for these points in rev B: signed firmware (secure boot), a defined update path, a software bill of materials and a vulnerability contact.

### 2.5 Substances: RoHS and REACH

- **RoHS, Directive 2011/65/EU.** This restricts lead, mercury, cadmium, hexavalent chromium, PBB, PBDE and four phthalates in electrical equipment. It is declared through the same CE marking and EU declaration of conformity.[11] **Action:** require RoHS declarations from the EMS and from every housing supplier, and keep them in the technical file.
- **REACH, Regulation (EC) 1907/2006.** If an article contains a Candidate List substance (SVHC) above 0.1 % w/w, you must give your customers safe-use information, and consumers on request within 45 days. You must also submit a notification to ECHA's **SCIP database** under the Waste Framework Directive.[12] **Action:** require REACH/SVHC statements from your suppliers. The highest risk is in cables, plastics, foams and the gasket.

### 2.6 WEEE: ElektroG registration with stiftung ear

- In Germany a producer must register **with device type and brand** before placing electrical equipment on the market. Unregistered producers' goods may not be offered by retailers or marketplaces.[13]
- You can only place equipment on the German market **after** you receive the registration notice. stiftung ear quotes **3–4 weeks** for processing. Products for private households also need an **insolvency-proof guarantee**.[15]
- A producer based outside Germany must appoint a German **authorised representative**.[16]
- Mark the device with the **crossed-out wheelie bin symbol** (ElektroG Annex 3) and make the producer identifiable. The marking must also show that the device was placed on the market after 13 August 2005 (in practice a solid bar under the bin symbol).[14] Put the WEEE registration number in offers and on invoices.[13]
- For devices that can be used in private households, enclose written information with each device: separate collection, free take-back by retailers, your own return options, deleting personal data, and the meaning of the bin symbol.[36]
- Registration is valid in Germany only. Each additional EU country needs its own WEEE registration.[30]

### 2.7 Packaging: PPWR, VerpackDG and LUCID

- The EU **Packaging and Packaging Waste Regulation (EU) 2025/40 (PPWR)** applies from 12 August 2026.[19] In Germany it is implemented by the **VerpackDG**, which covers registration (§ 6), system participation (§ 7) and data reporting (§ 9).[20]
- **Register in the LUCID packaging register** before you first place packaged goods on the market. Registration is free.[17] For sales and shipping packaging that ends up with private consumers, you also need a **system participation contract** with a dual-system operator, and you report the volumes to both the system operator and LUCID.[18]
- Keep the packaging simple: cardboard only, no plastic window, no foam. That keeps fees low and makes the PPWR recyclability requirements easy to meet.
- If the box is designed or made under your name or trade mark, the PPWR treats you as the **manufacturer of the packaging** (exception: if you are a micro-enterprise and the box supplier is in the same EU country, the supplier is the manufacturer). You then need a conformity assessment, technical documentation and an EU declaration of conformity for the packaging (PPWR Art. 15, 38, 39). Since 12 August 2026 this mainly covers the heavy-metal limit (lead, cadmium, mercury and hexavalent chromium together at most 100 mg/kg); more requirements follow from 2030.[19] Ask the box supplier for the material data you need.

### 2.8 CE marking, declaration of conformity and technical file

- **Technical documentation** (RED Annex V): description, schematics, layouts, BOM, risk assessment, test reports, a list of the standards applied, the module documents and the internet-connected assessment. Keep it, together with the EU declaration of conformity, **for 10 years** after the last unit is placed on the market.[1]
- **EU declaration of conformity** (Annex VI). It lists the RED and RoHS. Each unit must ship with a copy of the declaration or a **simplified declaration** that gives "the exact internet address where the full text … can be obtained".[1]
- **Product markings:** CE; a "type, batch or serial number"; your name or trade mark and postal address (on the packaging or an accompanying document if the device is too small).[1] Add the WEEE bin.[14]
- **Instructions and safety information** in German for Germany (the "language … determined by the Member State"). They must state the **frequency band** and the **maximum transmitted RF power**.[1] They should also specify the power supply (5 V, at least 1.5 A recommended by the design).
- **Series conformity:** you must have "procedures … in place for series production to remain in conformity". Any design change has to be reviewed against the declaration.[1]

### 2.9 General Product Safety Regulation (EU) 2023/988 (GPSR)

For products covered by the RED, the GPSR applies only to aspects the RED does not cover.[9] Two GPSR duties still apply to Bell Aid:

- **Online offers** must show the manufacturer's name and postal and electronic address, a picture, the type and identifier of the product, and any warnings.[9]
- **Accidents** caused by the product that lead to death or serious adverse health effects must be reported through the EU **Safety Business Gateway**.[9]

### 2.10 Power supply

Bell Aid runs from a standard USB charger. If you **bundle** a charger, buy a CE-marked unit that complies with the Ecodesign regulation for external power supplies, (EU) 2019/1782.[29] If you ship **without** a charger, the instructions must name the requirement (5 V USB, at least 1.5 A). The EU common-charger rules (USB-C, unbundling) apply only to listed categories such as phones and headphones. Bell Aid is not one of them, and its USB-C socket already matches.[1]

### 2.11 Outside the EU

- **United Kingdom (Great Britain):** CE-marked radio equipment that meets the EU requirements can be sold in GB; recognition of the CE marking has been extended indefinitely. UKCA marking is optional, including via a "fast-track" route based on the EU assessment.[25] Product labelling and instructions need English.
- **USA:** the module holds the FCC grant **FCC ID 2AC7Z-ESPS3MINI1** (Part 15C, single modular approval).[24] The host product typically needs a Part 15B (unintentional radiator) verification or self-declaration, the label text "Contains FCC ID: 2AC7Z-ESPS3MINI1" and the module's integration conditions.[28] For volume sales, budget a US test lab and a US importer.

---

## 3. DFM: from prototype to series

### 3.1 Electronics (board DBN-MAIN, rev A → rev B)

| # | Item | Why | Recommendation |
|---|---|---|---|
| 1 | **LED WS2812B-B/T (D1–D12) is discontinued** | LCSC stock 0, JLC stock 1 on 2026-11-01. The board cannot be assembled as designed. | Replace it with a 5050 drop-in, for example Worldsemi WS2812B-V5/W (LCSC C2874885) or XINGLIGHT XL-5050RGBC-2812B (LCSC C2843785). Verify pinout, footprint, timing and current against the datasheet, then confirm on a built board. Name an approved alternate in the BOM. |
| 2 | Universal population (mic and 12 LEDs on every board) | Each receiver carries an unused microphone, one of the costlier parts. | Two assembly variants from one PCB: **sender** (mic, 1 LED, R8 fitted) and **receiver** (no mic, 12 LEDs). |
| 3 | R8 role strap fitted by hand | Hand work costs money and causes errors. | Place R8 by machine in the sender variant, or set the role in the production test station. |
| 4 | Mounting holes made at Ø2.2 and drilled out by hand to Ø3.2 | Hand work | Specify Ø3.2 NPTH in the fab data. |
| 5 | Surface finish HASL | LGA parts (module, mic) need flat pads. | Consider ENIG for series. Decide together with the EMS. |
| 6 | Panelisation | Single 50 × 82 mm boards are slow to assemble. | Let the EMS define the panel (for example 2 × 3 with rails, fiducials and tooling holes). Keep the USB-C edge (receptacle flush with the board edge) and the antenna keepout clear of V-scores and break tabs. |
| 7 | Single-side assembly, 9 test pads on the back | Already production-friendly | Keep as is. These pads are the contact points for the test jig (section 5). |
| 8 | X-ray for LGA parts (U1 module, U4 mic) | Their joints are hidden under the part | Sample X-ray at FAI and after process changes. |
| 9 | MEMS mic acoustic port | Contamination makes it deaf. | Follow the mic datasheet's assembly notes (port protection, washing restrictions). |

**Parts availability and second sources** (current BOM, stock status of 2026-11-01):

- **Single-source, critical:** ESP32-S3-MINI-1-N8 (Espressif) and ICS-43434 (TDK). The mic was chosen because the first-choice mic had no stock. **Recommendation:** buy a buffer stock for the first batches (for example 6–12 months *(estimate)*) and ask the EMS to watch lifecycle status.
- **Multi-source:** USB-C receptacle, LDO, level shifter, ESD diode, PTC fuse, buttons and passives have pin-compatible equivalents from other makers. Add approved alternates to the BOM so the EMS can substitute without a redesign.
- **Rule:** no part enters the series BOM without at least two distributors with stock or a named alternate.

### 3.2 Housing: from FDM print to series

The prototype housing (case front, case back, diffuser, plunger/button cap, foot) is FDM-printed PLA with M3 self-tapping screws and a glued-in diffuser. PLA prints are fine for samples but not for sale: the material creeps under heat and load, the surface is visibly layered and it carries no flame rating.

| Route | Good for | Tooling *(estimate)* | Part cost per device set *(estimate)* | Lead time *(estimate)* |
|---|---|---|---|---|
| Industrial 3D printing (MJF/SLS PA12, SLA for the diffuser) | 10–500 units, market test | none | 8–25 € | 1–2 weeks |
| Vacuum casting (silicone moulds, PU resins) | 50–300 units, near-moulded look | 300–1,000 € per part | 5–15 € per part | 2–4 weeks |
| Injection moulding, aluminium tool | 500 to tens of thousands of units | 10,000–35,000 € for all parts | 0.8–2.5 € | 6–10 weeks to first shots |
| Injection moulding, hardened steel tool, multi-cavity | > 20,000 units/year | 30,000–80,000 € | 0.5–1.5 € | 8–12 weeks |

Family tools (several parts in one tool) reduce cost. Make sure the **tooling contract states that you own the tools**, how long they are guaranteed to last (in shots) and how they are released if you change supplier.

**Design-for-moulding changes (part of rev B):**

- Uniform walls (about 1.5–2.5 mm), 1–2° draft and no undercuts, or planned slides for the USB opening.
- Snap-fits or heat stakes instead of glue for the diffuser. Keep screws only if they serve a purpose.
- **Material:** for example PC/ABS or ABS for the shell and a translucent PC for the diffuser. The lab's EN IEC 62368-1 assessment decides whether a flame rating (UL 94 V-1/V-0) is required. Choosing a rated grade from the start avoids a re-test.
- **Antenna rule:** no metal, metallised paint or conductive fillers within 15 mm of the module antenna (the design rule from the delivered files).
- **Open items from the prototype review** to close in rev B: the device can tip over when the button is pressed (add weight or rubber feet), and the fit of the cable plug against the table still has to be checked with the real cable.

---

## 4. Finding and evaluating a manufacturer

### 4.1 Supply-chain models

1. **Turnkey EMS with box build** (PCBA, test, housing assembly, packing). One partner and one point of responsibility. EU-based EMS cost more per unit but are close to you and quick to audit.
2. **PCBA abroad, final assembly in the EU** (yourself or a fulfilment or assembly service). Low PCBA cost, but you take on assembly quality and logistics.
3. **Split sourcing** (PCBA from an EMS, housings from a moulder, packing elsewhere). Most flexible, but three interfaces to manage.

**Recommendation for the first 500–2,000 units:** model 1 or 2, with the housing supplier named and managed by the EMS.

Where to find partners: German trade fairs (productronica in Munich and embedded world in Nuremberg for EMS, Fakuma for moulders), referrals from your test lab, and EMS directories. Ask each candidate for two references from consumer electronics.

### 4.2 RFQ package (send the same to at least three suppliers)

- Fab data: Gerber or ODB++, drill files, stack-up, finish, panel requirements.
- Assembly data: pick-and-place file, assembly drawings per variant, BOM with MPNs, approved alternates and per-variant DNP list.
- Schematic (PDF) and the 3D board model (STEP).
- Housing: STEP files plus 2D drawings with critical dimensions, tolerances, material, colour and surface finish.
- Firmware: the release image with its SHA-256 checksum, the flashing procedure and the test specification (section 5).
- Packaging and label artwork: box, insert, quick-start guide, rating/CE/WEEE label.
- Commercial data: annual forecast, batch sizes (quote 3 tiers, for example 200 / 1,000 / 5,000), target price, Incoterms and required delivery date.
- Quality requirements: IPC-A-610 Class 2 workmanship, AQL levels, FAI, traceability level, and the RoHS/REACH declarations and certificate of conformity required with each delivery.

### 4.3 Evaluation and audit questions

- Valid ISO 9001 certificate, and what its scope covers.
- Process: solder-paste inspection, AOI and X-ray in-house? Moisture-sensitive-device handling? ESD programme?
- Component sourcing: authorised distributors only? How do they prevent counterfeits? How do they handle shortages and product change notices?
- Test: do they build and run your test jig? Do they log results per serial number? How do they program ESP32 parts?
- Traceability: lot level or serial level? How long are records kept?
- Quality: first-article inspection process, handling of non-conformities (8D), rework rules, typical first-pass yield.
- Capacity and stability: share of their business in your volume range, financial health, key staff.
- IP and data: NDA, who has access to firmware and design files, do they work for competitors?
- Commercials: **MOQ** (often driven by reels, minimum order values or mould shots), **NRE** (stencil, programming, fixtures, test jig, FAI, tooling), payment terms, price validity, ownership of tools and fixtures.

---

## 5. Production test and quality control

### 5.1 Test flow

| Stage | Check | Note |
|---|---|---|
| Incoming inspection | Part numbers, lot codes, moisture-sensitive packaging | Especially the module, the mic and the substitute LED |
| After reflow | SPI, AOI, sample X-ray of U1/U4 | Standard EMS process |
| **PCBA end-of-line test** (jig) | Flash, self-test, radio, LEDs, mic, button | See 5.2 and 5.3 |
| Final assembly | Power-on, LED ring, button, radio link with a golden unit, look and fit | 20–40 s per unit *(estimate)* |
| Outgoing QC | Sampling per ISO 2859-1 (AQL) | See 5.4 |

### 5.2 Firmware flashing in production

- The ESP32-S3 programs over its **native USB on the USB-C connector**. No extra programming chip is needed. The delivered build produces one merged image plus a checksum file. The station flashes it with `esptool`, verifies the checksum and logs it per unit.
- **Before series, decide whether to enable flash encryption and secure boot.** Both are one-time eFuse settings. Espressif recommends enabling both for production use and describes host-based production workflows for them.[26] They also support the CRA planning in 2.4.
- Assign the role in the station (sender or receiver), or let the R8 strap set it on the sender variant.
- Recommended firmware addition in rev B: a dedicated `selftest` console command that returns machine-readable results. The current firmware already has a console (`status`, `ring`, `monitor`) to build on.

### 5.3 Test jig (end-of-line)

- A bed-of-nails fixture with pogo pins on the **9 back-side test pads** (3V3, 5V, GND, UART, EN, GPIO0, spare GPIOs) plus a USB-C plug for power and flashing.
- **Tests:** supply current at idle and with LEDs on, within limits; 3V3 rail voltage; all 12 pixels in R/G/B (camera or light sensor); button; microphone response to a reference tone from a small speaker in the jig; ESP-NOW exchange with a golden unit with a minimum RSSI; final configuration (radio mode as certified, pairing cleared).
- Typical cost of jig and test software: 1,500–6,000 € *(estimate)*, often charged as NRE by the EMS.

### 5.4 AQL sampling, first-article inspection, traceability

- **AQL:** ISO 2859-1 (2026 edition) sets the sampling plans for lot-by-lot inspection.[27] Common practice in consumer electronics, not a legal requirement: critical 0, major 0.65–1.0, minor 2.5–4.0 at general inspection level II.
- **First-article inspection (FAI)** for each new build, new tool or part change: full dimensional report of moulded parts (T1 samples) against the drawing, a 100 % functional test of the first units, a BOM check (correct LED substitute and orientation), X-ray of the LGA parts and a comparison with a signed **golden sample**. No mass run before written FAI approval.
- **Traceability:** the RED requires a type, batch or serial number on each unit.[1] Use the module's unique **MAC address as the serial number**. Print it, with the model and date code, on the rating label, and link it to the test log, the firmware checksum and the component lot codes (module, mic). Keep a register of complaints and returns. The RED expects this "when deemed appropriate" with regard to the risks.[1]

---

## 6. Costs and timeline

### 6.1 Unit cost today and the main cost drivers

- **Today:** at small batches, the cost of a sender + receiver pair is above the 20 € target.
- **Main drivers:** the ESP32-S3 module, the ICS-43434 microphone (currently fitted to every board) and the fixed assembly fees per order.
- **Levers:** role-specific population (section 3.1, item 2), volume prices for the module and microphone, and moulded housings. Together they are the route to the target. Confirm with real RFQ quotes.
- Current cost figures are available from Makehare on request. Charger, cable and packaging are not included in these cost considerations.

### 6.2 Phases, duration and budget *(all estimates)*

| Phase | Content | Duration | Budget |
|---|---|---|---|
| A. Design revision B | LED change, variants, DFM fixes, housing for the chosen process, self-test firmware, CRA preparation | 3–6 weeks | Design fee, fixed-price quote |
| B. Engineering samples | 20–50 units from the future EMS, printed or cast housings, internal verification | 3–5 weeks | 800–3,000 € |
| C. Compliance | Pre-compliance EMC scan, then accredited tests: EN 300 328 (module reuse), EN 301 489-1/-17, EN IEC 62368-1, EN 62479; technical file and declaration | 4–8 weeks (book lab slots early) | Pre-scan 1,000–2,500 €; full tests 5,000–15,000 € |
| D. Tooling (optional at start) | Moulds, T1 samples, corrections T2 | 8–14 weeks | 10,000–35,000 € (aluminium) or 30,000–80,000 € (steel) |
| E. Test jig and process | Fixture, station software, work instructions | 3–6 weeks, parallel to C/D | 1,500–6,000 € |
| F. Registrations | WEEE (stiftung ear, incl. guarantee), LUCID, packaging system contract, accounts for other countries | 3–6 weeks, parallel | a few hundred to ~1,500 € in year 1 |
| G. Pilot run | 100–300 units, FAI, yield analysis, packaging check | 3–5 weeks | Units plus about 1,000–3,000 € NRE |
| **Total to first sellable batch** | | **about 3–4 months** with printed or cast housings; **about 5–8 months** with moulds | **about 10,000–25,000 €** without moulds; **about 25,000–100,000 €** with moulds, plus stock |

---

## 7. First 10 steps

1. **Fix the product claim and name.** Write the intended purpose as a household doorbell signal, not a medical device (section 1), and use the same wording on the label, in the manual, on the website and in listings.
2. **Choose the market-entry volume and the housing route** (printed or cast vs. moulded) from section 6. This choice drives budget and timeline.
3. **Order design revision B:** replacement LED with an approved alternate, sender/receiver variants, removal of hand work, housing DFM, `selftest` command, secure-boot/CRA decision.
4. **Freeze the radio configuration** (channel, power, Long Range mode on or off) and get the module's CE test report set and integration conditions from Espressif.
5. **Send the RFQ package** (section 4.2) to at least three EMS and, if you mould, three moulders. Audit the top two with the questions in 4.3.
6. **Build 20–50 engineering samples** at the chosen EMS and verify range, detection, LED current and thermal behaviour on real doorbells.
7. **Book the accredited lab.** Run a pre-compliance EMC scan first, then the full tests on samples built the series way.
8. **Register:** apply to stiftung ear (with the guarantee) and LUCID, and sign a packaging system contract. You may not sell in Germany before the WEEE registration notice arrives.
9. **Compile the technical file and sign the EU declaration of conformity.** Finalise the label (CE, WEEE bin, model, serial, address), the German manual (frequency band and maximum power) and the GPSR data for online listings.
10. **Run the pilot batch with FAI and the end-of-line jig.** Approve the golden sample, then release mass production with AQL outgoing inspection, serial traceability and a complaints register.

---

## 8. Open points to confirm

- **Medical device boundary:** we found no entry for visual doorbell or alarm signallers in the EU borderline manual.[31] Our assessment rests on the MDR definition, UK guidance and German market practice.[7][8][35] Keep all marketing free of medical claims.
- **Internet-connected / CRA scope:** these are our readings of the texts. They need confirming against the final firmware feature set (no IP, no OTA) and any later CRA guidance.[2][6]
- **WEEE device type:** which stiftung ear device type fits Bell Aid. stiftung ear decides this at registration.[15] Our reading: a small household device (category 5, "small equipment that can be used in private households") rather than small IT and telecommunications equipment (category 6), because its purpose is signalling, not processing or transmitting information as such.
- **Housing material (fire safety):** whether EN IEC 62368-1 requires a flame-rated housing material for this 5 V USB-powered design. The standard sets fire safeguards by power source class: a circuit that can draw at most 15 W (PS1) needs none; up to 100 W (PS2) it needs flame-rated boards and flame-rated or separated material near possible ignition sources, which can include the housing; only above that (PS3) a fire enclosure. A generic USB charger can deliver more than 15 W, so the lab may treat the board as PS2 unless the input is limited (for example by the on-board fuse). The test lab decides. Choosing a rated grade (for example UL 94 V-1 or better) for the series housing avoids a re-test.
- **Reimbursement:** German statutory health insurers already list doorbell signal systems as aids (product group 13.99.04, "Signalanlagen").[34] A listing needs an application to the GKV-Spitzenverband with proof of function, safety and quality and, where required, medical benefit.[37] It is possible in principle but would make hearing loss the core positioning, so the claim wording (section 1) needs extra care. A business decision, not a requirement.

---

## Sources

1. Directive 2014/53/EU (RED), consolidated version 30/05/2026, EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:02014L0053-20260530
2. European Commission, Guide to the Radio Equipment Directive 2014/53/EU (version 07.07.2026): https://single-market-economy.ec.europa.eu/document/download/a0c7e4c5-27db-4c59-9c43-a380ed21563f_en?filename=RED%20Guide%20-%20version%2007.07.2026%20-%20with%20COVER.pdf
3. European Commission, harmonised standards for radio equipment (RED), incl. summary list: https://single-market-economy.ec.europa.eu/single-market/european-standards/harmonised-standards/radio-equipment_en
4. European Commission, harmonised standards under the Low Voltage Directive (incl. EN 62368-1, EN 62311, EN 62479): https://single-market-economy.ec.europa.eu/single-market/goods/european-standards/harmonised-standards/low-voltage-lvd_en
5. Commission Delegated Regulation (EU) 2022/30, EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32022R0030
6. Regulation (EU) 2024/2847 (Cyber Resilience Act), EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32024R2847
7. Regulation (EU) 2017/745 (Medical Device Regulation), Art. 2, EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32017R0745
8. MHRA, Borderlines with medical devices and other products in Great Britain, section 5: https://www.gov.uk/government/publications/borderlines-with-medical-devices/borderlines-with-medical-devices-and-other-products-in-great-britain
9. Regulation (EU) 2023/988 (GPSR), Art. 2, 19, 20, EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32023R0988
10. Regulation (EU) 2019/1020 (market surveillance), Art. 4, EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32019R1020
11. Directive 2011/65/EU (RoHS), consolidated, EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:02011L0065-20250101
12. ECHA, Obligations from Candidate List substances (REACH Art. 33, SCIP): https://echa.europa.eu/candidate-list-obligations
13. ElektroG § 6 (registration), gesetze-im-internet.de: https://www.gesetze-im-internet.de/elektrog_2015/__6.html
14. ElektroG § 9 (marking), gesetze-im-internet.de: https://www.gesetze-im-internet.de/elektrog_2015/__9.html
15. stiftung ear, Applying for WEEE registration: https://www.stiftung-ear.de/en/guides/applying-for-weee-registration/
16. stiftung ear, WEEE registration as a foreign company: https://www.stiftung-ear.de/en/guides/applying-for-weee-registration-as-a-foreign-company
17. Zentrale Stelle Verpackungsregister, LUCID registration: https://www.verpackungsregister.org/registrierung/alle-informationen-zur-registrierung
18. Zentrale Stelle Verpackungsregister, system participation and data reporting: https://www.verpackungsregister.org/systembeteiligung-und-datenmeldung/ueberblick
19. Regulation (EU) 2025/40 (PPWR), EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32025R0040
20. Verpackungsrecht-Durchführungsgesetz (VerpackDG), gesetze-im-internet.de: https://www.gesetze-im-internet.de/verpackdg/
21. Bundesnetzagentur, general frequency assignments (Vfg. 128/2023, 2400–2483.5 MHz): https://www.bundesnetzagentur.de/DE/Fachthemen/Telekommunikation/Frequenzen/Allgemeinzuteilungen/start.html
22. Bundesnetzagentur, market surveillance results 2025 (press release, 5 Feb 2026): https://www.bundesnetzagentur.de/SharedDocs/Pressemitteilungen/EN/2026/20260205_Marktueberwachung.html
23. Espressif, Certifications and compliance documents: https://www.espressif.com/en/support/documents/certificates
24. FCC grant for ESP32-S3-MINI-1, FCC ID 2AC7Z-ESPS3MINI1: https://documentation.espressif.com/ESP32-S3-MINI-1%20FCC%20Certification.pdf
25. GOV.UK, Radio Equipment Regulations 2017: Great Britain: https://www.gov.uk/government/publications/radio-equipment-regulations-2017/radio-equipment-regulations-2017-great-britain
26. Espressif, ESP32-S3 Security Features Enablement Workflows: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/security/security-features-enablement-workflows.html
27. ISO 2859-1:2026, Sampling procedures for inspection by attributes: https://www.iso.org/standard/85464.html
28. 47 CFR Part 15 (FCC radio frequency devices), eCFR: https://www.ecfr.gov/current/title-47/chapter-I/subchapter-A/part-15
29. Commission Regulation (EU) 2019/1782 (ecodesign, external power supplies), EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32019R1782
30. stiftung ear, Obligations as a producer (FAQ: registration valid only in Germany): https://www.stiftung-ear.de/en/topic-areas/find-out-your-obligation-as-a-producer/
31. MDCG, Manual on borderline and classification under Regulations (EU) 2017/745 and 2017/746: https://health.ec.europa.eu/document/download/71a87df8-5ca1-4555-b453-b65bdf8de909_en?filename=md_borderline_manual_en.pdf
32. Commission Delegated Regulation (EU) 2026/339 repealing Delegated Regulation (EU) 2022/30, EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32026R0339
33. Commission Implementing Regulation (EU) 2025/2392 (technical description of important and critical products under the CRA), EUR-Lex: https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32025R2392
34. GKV-Spitzenverband, Hilfsmittelverzeichnis product group 13 (incl. 13.99.04 Signalanlagen): https://www.gkv-spitzenverband.de/media/dokumente/krankenversicherung_1/hilfsmittel/fortschreibungen_aktuell/07_2024/2024_07_29_Fortschreibung_Produktgruppe_13_Hoerhilfen.pdf
35. Humantechnik, EU declaration of conformity A-2465-0 (radio doorbell transmitter, aids register no. 13.99.04.0045): https://www.humantechnik.com/downloads/pdf/eu_konf/CE2465-0-V2.pdf
36. ElektroG § 18 (information for private households), gesetze-im-internet.de: https://www.gesetze-im-internet.de/elektrog_2015/__18.html
37. SGB V § 139 (aids register), gesetze-im-internet.de: https://www.gesetze-im-internet.de/sgb_5/__139.html
