---
status: accepted
---

# Das Physikpaket rechnet in SI-Einheiten und ISO-8855-Fahrzeugachsen, nicht im Unreal-System

Unreal rechnet in Zentimetern mit linkshändigen Achsen. Das Physikpaket rechnet trotzdem in SI-Einheiten (m, kg, s, rad) als `double` mit Fahrzeugachsen nach ISO 8855 (X vorn, Y links, Z oben, rechtshändig), weil alle Formeln und Quellen der Fahrzeugdynamik so arbeiten und das Paket keine Unreal-Abhängigkeit haben darf (ADR 0003). Die Umrechnung macht ausschließlich der Unreal-Adapter.
