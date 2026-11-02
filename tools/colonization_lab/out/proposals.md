# Предложения Claude (PROPOSAL) и скрытые / неизвестные факты

Генерируется `lab.py` из статусов `canon/*.json` (knowledge.py). CANON = есть в заметках автора; PROPOSAL = добавлено/пересчитано Claude или является результатом модели; UNKNOWN = намеренно без ответа. Видимость: PUBLIC_RECORD / AUTHOR_TRUTH.

Счётчики: AUTHOR_TRUTH 18, CANON 309, PROPOSAL 290, PUBLIC_RECORD 590, UNKNOWN 9

Все выходы модели (население 3126, экономика, коридоры, технологии) — PROPOSAL (model) и в этот список не перечисляются поштучно.

## beacons (24)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| beacons/8@2755 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/9@2760 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | eps_eri_harmony |
| beacons/13@2765 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/14@2775 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/30@2778 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/12@2785 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | gj832_tianhe |
| beacons/31@2790 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | cyg61_kern |
| beacons/22@2795 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | delta_pav_freekarma |
| beacons/4@2800 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | lalande_hellas |
| beacons/11@2800 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/10@2805 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | gj581_europa |
| beacons/57@2840 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | hd20794_hellas2 |
| beacons/15@2850 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | hd40307_maland |
| beacons/3@2855 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | wolf359_citadel |
| beacons/23@2880 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | cyg61_kern |
| beacons/53@2810 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/54@2860 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/25@2860 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/16@2860 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | hd69830_verde |
| beacons/17@2905 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/37@3040 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | cyg61_kern |
| beacons/45@3045 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | cyg61_kern |
| beacons/41@3050 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | tau_ceti_cognium |
| beacons/0@3112 | PROPOSAL | PUBLIC_RECORD | год и оператор маяка — реконсиляция | sol_authority |

## colonies (128)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| colonies/tau_ceti_cognium/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/tau_ceti_cognium/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/tau_ceti_cognium/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/tau_ceti_cognium/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/tau_ceti_cognium/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/proxima_outpost/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/proxima_outpost/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/proxima_outpost/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/proxima_outpost/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/proxima_outpost/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/gj832_tianhe/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/gj832_tianhe/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/gj832_tianhe/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/gj832_tianhe/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/gj832_tianhe/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/gj832_tianhe/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/gj832_gaia_zone/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/gj832_gaia_zone/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/gj832_gaia_zone/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/gj832_gaia_zone/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/gj832_gaia_zone/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/gj832_gaia_zone/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/lalande_hellas/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/lalande_hellas/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/lalande_hellas/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/lalande_hellas/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/lalande_hellas/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/lalande_hellas/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/lalande_mecca/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/lalande_mecca/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/lalande_mecca/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/lalande_mecca/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/lalande_mecca/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/eps_eri_harmony/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/eps_eri_harmony/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/eps_eri_harmony/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/eps_eri_harmony/pop_3126_authored | PROPOSAL | PUBLIC_RECORD | реконсиляция (разделение/пересчёт) | pop_3126_authored |
| colonies/eps_eri_harmony/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/eps_eri_harmony/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/eps_eri_strife | PROPOSAL | PUBLIC_RECORD | основатель FREE-VENTURE-1: PROPOSAL | Пояс Раздора / Вольные астероиды |
| colonies/eps_eri_strife/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/eps_eri_strife/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/eps_eri_strife/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/eps_eri_strife/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/eps_eri_strife/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/eps_eri_strife/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/wolf359_citadel/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/wolf359_citadel/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/wolf359_citadel/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/wolf359_citadel/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/wolf359_citadel/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/gj581_europa/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/gj581_europa/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/gj581_europa/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/gj581_europa/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/gj581_europa/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/gj581_freeclans/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/gj581_freeclans/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/gj581_freeclans/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/gj581_freeclans/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/gj581_freeclans/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/gj581_freeclans/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/hd20794_hellas2/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/hd20794_hellas2/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/hd20794_hellas2/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/hd20794_hellas2/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/hd20794_hellas2/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/hd20794_newindia/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/hd20794_newindia/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/hd20794_newindia/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/hd20794_newindia/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/hd20794_newindia/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/hd40307_maland/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/hd40307_maland/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/hd40307_maland/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/hd40307_maland/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/hd40307_maland/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/hd40307_maland/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/hd40307_firmament/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/hd40307_firmament/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/hd40307_firmament/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/hd40307_firmament/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/hd40307_firmament/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/hd40307_firmament/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/hd69830_verde/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/hd69830_verde/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/hd69830_verde/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/hd69830_verde/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/hd69830_verde/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/hd69830_verde/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/delta_pav_karma/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/delta_pav_karma/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/delta_pav_karma/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/delta_pav_karma/pop_3126_authored | PROPOSAL | PUBLIC_RECORD | реконсиляция (разделение/пересчёт) | pop_3126_authored |
| colonies/delta_pav_karma/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/delta_pav_karma/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/delta_pav_freekarma/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/delta_pav_freekarma/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/delta_pav_freekarma/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/delta_pav_freekarma/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/delta_pav_freekarma/pop_3126_authored | PROPOSAL | PUBLIC_RECORD | реконсиляция (разделение/пересчёт) | pop_3126_authored |
| colonies/delta_pav_freekarma/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/delta_pav_freekarma/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/gj876_next/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/gj876_next/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/gj876_next/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/gj876_next/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/gj876_next/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/gj876_next/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/cyg61_kern/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/cyg61_kern/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/cyg61_kern/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/cyg61_kern/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/cyg61_kern/pop_3126_authored | PROPOSAL | PUBLIC_RECORD | реконсиляция (разделение/пересчёт) | pop_3126_authored |
| colonies/cyg61_kern/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/cyg61_kern/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/hd114783_hellas3/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/hd114783_hellas3/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/hd114783_hellas3/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/hd114783_hellas3/pop_3126_authored | PROPOSAL | PUBLIC_RECORD | реконсиляция (разделение/пересчёт) | pop_3126_authored |
| colonies/hd114783_hellas3/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/hd114783_hellas3/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |
| colonies/leo83_zion/assets | PROPOSAL | PUBLIC_RECORD | активы и их важность — реконсиляция/экономика | assets |
| colonies/leo83_zion/culture_drift | PROPOSAL | PUBLIC_RECORD | гипотеза Colonycore v2 | culture_drift |
| colonies/leo83_zion/demography | PROPOSAL | PUBLIC_RECORD | входы причинной демографии (допущения F1–F12) | demography |
| colonies/leo83_zion/name | PROPOSAL | PUBLIC_RECORD | имя дано реконсиляцией / старой веткой | name |
| colonies/leo83_zion/pop_3126_model | PROPOSAL | PUBLIC_RECORD | результат модели demography.py, не канон | pop_3126_model |
| colonies/leo83_zion/regime_3126 | PROPOSAL | PUBLIC_RECORD | машинная метка режима по досье (Claude) | regime_3126 |

## demography_events (20)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| demography_events/2030/earth_south/mortality | PROPOSAL | PUBLIC_RECORD | history.md (CANON): климатические беженцы, локальные войны; величина — допущение | history.md (CANON): климатические беженцы, локальные войны; величина — допущение |
| demography_events/2030/earth_south/capacity_loss | PROPOSAL | PUBLIC_RECORD | history.md (CANON): климат; величина — допущение | history.md (CANON): климат; величина — допущение |
| demography_events/2470/earth_north/capacity_loss | PROPOSAL | PUBLIC_RECORD | FINAL-ARK-1 (CANON): климат уничтожил прибрежные города; величина — допущение | FINAL-ARK-1 (CANON): климат уничтожил прибрежные города; величина — допущение |
| demography_events/2470/earth_south/capacity_loss | PROPOSAL | PUBLIC_RECORD | FINAL-ARK-1 (CANON): климат уничтожил прибрежные города; величина — допущение | FINAL-ARK-1 (CANON): климат уничтожил прибрежные города; величина — допущение |
| demography_events/2485/earth_north/mortality | PROPOSAL | PUBLIC_RECORD | FINAL-ARK-1 (CANON): войны за остатки ресурсов, эпидемии; величина — допущение | FINAL-ARK-1 (CANON): войны за остатки ресурсов, эпидемии; величина — допущение |
| demography_events/2485/earth_south/mortality | PROPOSAL | PUBLIC_RECORD | FINAL-ARK-1 (CANON): войны, эпидемии; величина — допущение | FINAL-ARK-1 (CANON): войны, эпидемии; величина — допущение |
| demography_events/2485/earth_north/tech_loss | PROPOSAL | PUBLIC_RECORD | разваливающаяся цивилизация 2500 (CANON): разрыв цепочек поставок; величина — допущение | разваливающаяся цивилизация 2500 (CANON): разрыв цепочек поставок; величина — допущение |
| demography_events/2485/earth_south/tech_loss | PROPOSAL | PUBLIC_RECORD | то же | то же |
| demography_events/3112/earth_north/threat | PROPOSAL | PUBLIC_RECORD | Земля запускает маяк и флотскую программу (реконсиляция) | Земля запускает маяк и флотскую программу (реконсиляция) |
| demography_events/3112/earth_south/threat | PROPOSAL | PUBLIC_RECORD | то же | то же |
| demography_events/2420/delta_pav_karma/transfer | PROPOSAL | PUBLIC_RECORD | DHARMA-1 2420 исход «нечистых» (CANON); доля 25 % — допущение | DHARMA-1 2420 исход «нечистых» (CANON); доля 25 % — допущение |
| demography_events/2420/delta_pav_karma/threat | PROPOSAL | PUBLIC_RECORD | холодная война со Свободной Кармой | холодная война со Свободной Кармой |
| demography_events/2420/delta_pav_freekarma/threat | PROPOSAL | PUBLIC_RECORD | то же | то же |
| demography_events/2575/eps_eri_harmony/transfer | PROPOSAL | PUBLIC_RECORD | HOMEWARD-1 2575 «Вольные астероиды» (CANON событие); 600 беглецов — допущение | HOMEWARD-1 2575 «Вольные астероиды» (CANON событие); 600 беглецов — допущение |
| demography_events/2575/eps_eri_harmony/threat | PROPOSAL | PUBLIC_RECORD | тлеющая война с Поясом Раздора | тлеющая война с Поясом Раздора |
| demography_events/2580/hd20794_hellas2/deaths | PROPOSAL | PUBLIC_RECORD | Война за воду: потери второй стороны — допущение | Война за воду: потери второй стороны — допущение |
| demography_events/2680/hd40307_maland/threat | PROPOSAL | PUBLIC_RECORD | холодная война с роем/Твердыней | холодная война с роем/Твердыней |
| demography_events/2742/hd40307_firmament/threat | PROPOSAL | PUBLIC_RECORD | то же | то же |
| demography_events/2355/wolf359_citadel/threat | PROPOSAL | PUBLIC_RECORD | травма IRON-SHIELD-2 (CANON) -> паранойя | травма IRON-SHIELD-2 (CANON) -> паранойя |
| demography_events/2480/gj832_gaia_zone/tfr_mult | PROPOSAL | PUBLIC_RECORD | GREEN-ARK-1 2480: 70 % «заражены покоем» (CANON событие); снижение рождаемости — допущение | GREEN-ARK-1 2480: 70 % «заражены покоем» (CANON событие); снижение рождаемости — допущение |

## earth_3126 (1)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| earth_3126 | PROPOSAL | PUBLIC_RECORD | Земля 3126 — предложение для автора (§10 справочника) | Солнечная Администрация (Sol Authority) |

## extra_links (1)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| extra_links/60-3 | PROPOSAL | PUBLIC_RECORD | реконсиляция | нелицензионная линия Сварог — Цитадель: два отказника торгуют напрямую |

## frontier_pools (4)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| frontier_pools/gj674_frontier | PROPOSAL | PUBLIC_RECORD | фронтир у маяка (вход демографии, Claude) | Фронтир Gliese 674 |
| frontier_pools/hd85512_frontier | PROPOSAL | PUBLIC_RECORD | фронтир у маяка (вход демографии, Claude) | Фронтир HD 85512 |
| frontier_pools/gj357_frontier | PROPOSAL | PUBLIC_RECORD | фронтир у маяка (вход демографии, Claude) | Фронтир Gliese 357 |
| frontier_pools/hd104067_frontier | PROPOSAL | PUBLIC_RECORD | фронтир у маяка (вход демографии, Claude) | Фронтир HD 104067 |

## jump_chronology (11)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| jump_chronology/2583 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Академия Нового Шанхая публикует теорему о схлопывании интервала; сигнал доходит до Земли в 2595 — отсюда «изобретён в 2 |
| jump_chronology/2752 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | HYPERION-2: гиперпространство — среда с градиентами; без маяков прыжок — лотерея; изобретён маяк |
| jump_chronology/2755 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Маяк Тау Кита; 2760 — Эпсилон Эридана (Гармония первая получает связь, 5.5 св. года) |
| jump_chronology/2765 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Концессия GJ 876: Провидение отдаёт гиганты Когниуму в обмен на вычислительное ядро нового поколения |
| jump_chronology/2790 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Конвенция Маяков: лицензии, стандарты, инспекции. «Кордон Сола» — Земля исключена из сети |
| jump_chronology/2790 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Гипер-разведка находит обломки UBUNTU-1 в поясе GJ 667. Тел нет |
| jump_chronology/2810 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Лотерейный прыжок к HD 69830: «последние люди Вселенной» узнают, что они не последние |
| jump_chronology/2862 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Кланы Свободного мира выкладывают спецификации привода «для всех»: эпоха дешёвых нелицензионных прыжков и пиратства |
| jump_chronology/2905 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Маяк у TRAPPIST-1; 2910 — две разведгруппы «остаются»; карантин |
| jump_chronology/3040 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Керн и Когниум ставят экспедиционные маяки в глубоком кусте (HD 114783, 83 Leonis, HD 104067) — без связи с ядром (разры |
| jump_chronology/3112 | PROPOSAL | PUBLIC_RECORD | реконсиляция (часть III) | Земля запускает собственный маяк и флотскую программу |

## jump_technology (11)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| jump_technology/tau_ceti_cognium@2750 | CANON | AUTHOR_TRUTH | HYPERION-1 (CANON); ядра — гос. тайна Управления Маяков (PROPOSAL) | HYPERION-1 (CANON); ядра — гос. тайна Управления Маяков (PROPOSAL) |
| jump_technology/eps_eri_harmony@2760 | PROPOSAL | PUBLIC_RECORD | маяк 2760; ядра покупает у Когниума | маяк 2760; ядра покупает у Когниума |
| jump_technology/gj876_next@2765 | PROPOSAL | PUBLIC_RECORD | концессия GJ 876 | концессия GJ 876 |
| jump_technology/gj832_tianhe@2785 | PROPOSAL | PUBLIC_RECORD | маяк 2785 | маяк 2785 |
| jump_technology/cyg61_kern@2790 | PROPOSAL | PUBLIC_RECORD | маяк 2790 | маяк 2790 |
| jump_technology/delta_pav_freekarma@2795 | PROPOSAL | PUBLIC_RECORD | маяк 2795 | маяк 2795 |
| jump_technology/lalande_hellas@2800 | PROPOSAL | PUBLIC_RECORD | маяк 2800 | маяк 2800 |
| jump_technology/lalande_mecca@2800 | PROPOSAL | PUBLIC_RECORD | собственная математика прыжка — вне стандарта Когниума | собственная математика прыжка — вне стандарта Когниума |
| jump_technology/gj581_europa@2805 | PROPOSAL | PUBLIC_RECORD | маяк 2805 | маяк 2805 |
| jump_technology/eps_eri_strife@2862 | PROPOSAL | PUBLIC_RECORD | пиратство после утечки 2862 | пиратство после утечки 2862 |
| jump_technology/proxima_outpost@2862 | PROPOSAL | PUBLIC_RECORD | отказ от лицензии (CANON); пиратские копии после 2862 — допущение | отказ от лицензии (CANON); пиратские копии после 2862 — допущение |

## missions (86)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| missions/MAYFLOWER-2/anchors/2180/sabotage_alleged/hidden_truth | PROPOSAL | AUTHOR_TRUTH | объяснение реконсиляции; публично — официальная версия | deferred_maintenance: Совет урезал бюджет запчастей контуров охлаждения в 2171, вину переложили на каторжников |
| missions/GENESIS-1 | PROPOSAL | PUBLIC_RECORD | реконсиляция A2 (идея старой ветки, числа Claude) | SpaceArk consortium: Singapore + Shanghai diaspora capital + Pacific tech, engineers who left both Beijing and Washingto |
| missions/GENESIS-1/arrival_population | PROPOSAL | PUBLIC_RECORD | модель вахты/крио (реконсиляция C2) | arrival_population |
| missions/GENESIS-1/arrival_year | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | arrival_year |
| missions/GENESIS-1/fate | PROPOSAL | PUBLIC_RECORD | fate.md / досье | fate |
| missions/GENESIS-1/launch_year | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | launch_year |
| missions/GENESIS-1/population | PROPOSAL | PUBLIC_RECORD | число из досье (реконсиляция C1: стартовые численности по S1) | population |
| missions/GENESIS-1/anchors/2263/arrival | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | Прибытие к Тау Кита; «Старшие семьи» Мэйфлауэра против «Новой крови» |
| missions/GENESIS-1/anchors/2270/merger | PROPOSAL | PUBLIC_RECORD | событие миссии-предложения | Слияние двух колоний; главный хаб GENESIS-1 даёт имя городу — «Новый Шанхай» |
| missions/UBUNTU-1/fate | UNKNOWN | AUTHOR_TRUTH | неизвестно даже автору | fate |
| missions/UBUNTU-1/anchors/2790/found | PROPOSAL | PUBLIC_RECORD | реконсиляция B20: обломки найдены гипер-разведкой | Гипер-разведка Когниума находит обломки в поясе GJ 667. Тел нет. |
| missions/CELESTIAL-DESTINY-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2172 | arrival_year |
| missions/CELESTIAL-DESTINY-1/anchors/2212/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2172 | Эпсилон Эридана: поселения на спутниках eps Eri b и в поясе |
| missions/GREEN-ARK-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2190 | arrival_year |
| missions/GREEN-ARK-1/fate | PROPOSAL | AUTHOR_TRUTH | скрытая правда (реконсиляция): симбионт из корабельного микробиома | fate |
| missions/GREEN-ARK-1/anchors/2270/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2190 | GJ 832: сад-купола на GJ 832 b I и в поясе («Гайя-Прим») |
| missions/GREEN-ARK-1/anchors/2480/ecology_failure/hidden_truth | PROPOSAL | AUTHOR_TRUTH | объяснение реконсиляции; публично — официальная версия | симбионт вырос из корабельного микробиома («успокаивающий» штамм биотех-партнёра) |
| missions/GREEN-ARK-1/anchors/2510/quarantine | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Тяньхэ объявляет зону «Филоморфоз» |
| missions/DHARMA-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2210 | arrival_year |
| missions/DHARMA-1/anchors/2183/class_conflict | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Бунт шудр: доступ к генному массиву, конец неприкасаемости; подавлен кшатриями |
| missions/DHARMA-1/anchors/2361/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2210 | Дельта Павлина: купола у Delta Pavonis h |
| missions/IRON-SHIELD-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2185 | arrival_year |
| missions/IRON-SHIELD-1/anchors/2215/class_conflict | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Восстание каторжников за статус детей; подавлено ЧВК |
| missions/IRON-SHIELD-1/anchors/2228/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2185 | Вольф 359: астероид-крепость (пояс-диск) |
| missions/AL-QUDS-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2195 | arrival_year |
| missions/AL-QUDS-1/anchors/2243/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2195 | Lalande 21185 — система уже занята Новой Элладой (с 2197) |
| missions/NEXT-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2230 | arrival_year |
| missions/NEXT-1/anchors/2218/purge | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Отступники / Чистые / Еретики подавлены Советом Архитекторов и ИИ |
| missions/NEXT-1/anchors/2314/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2230 | GJ 876: жёсткая посадка модуля на спутник GJ 876 b I; ядро ИИ уцелело |
| missions/EUROPA-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2325 | arrival_year |
| missions/EUROPA-1/anchors/2318/reform | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Конфликт поколений; комиссия меняет цвет бюллетеней |
| missions/EUROPA-1/anchors/2377/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2325 | GJ 581: орбитальная колонизация (g не подтверждена) |
| missions/RESOURCER-1/arrival_population | PROPOSAL | PUBLIC_RECORD | модель вахты/крио (реконсиляция C2) | arrival_population |
| missions/NEW-HELLAS-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2380 | arrival_year |
| missions/NEW-HELLAS-1/anchors/2365/sleeper_return | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Спящие владельцы просыпаются с идеологией 2260 и встречают коллективистов-корабельных |
| missions/NEW-HELLAS-1/anchors/2384/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2380 | HD 20794: спутники гиганта d |
| missions/NEW-JERUSALEM-1/anchors/2291/target_change | PROPOSAL | PUBLIC_RECORD | реконсиляция B9: смена цели на разгоне | Земля сообщает: цель GJ 667 C c не ледяная луна. Синод на разгоне меняет цель на 83 Leonis (Лев от колена Иудина); крейс |
| missions/NEW-JERUSALEM-1/anchors/2375/sleeper_return | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Пробуждение высшего клира: конфликт с корабельными и компромисс |
| missions/UHURU-2/arrival_year | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | arrival_year |
| missions/UHURU-2/anchors/2276/launch_disaster | PROPOSAL | PUBLIC_RECORD | реконсиляция A4: пожар в доке объясняет номер «-2» | Пожар в доке L1 уничтожает корпус UHURU-1 (600 рабочих); слухи о саботаже конкурентов не подтверждены |
| missions/UHURU-2/anchors/2542/arrival | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | HD 40307: купола и сельскохозяйственные кольца |
| missions/ODYSSEY-1/anchors/2600/sleeper_return | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Патриции просыпаются: «вам капитал, нам социальная сфера» — 20 лет судов вместо стрельбы |
| missions/NIRVANA-1/fate | UNKNOWN | AUTHOR_TRUTH | неизвестно даже автору | fate |
| missions/AMAZONIA-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2570 | arrival_year |
| missions/AMAZONIA-1/anchors/2405/contact_lost | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Отказ антенны дальней связи; Земля молчит — экипаж решает, что Земли больше нет |
| missions/AMAZONIA-1/anchors/2576/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2570 | HD 69830: колония «последних людей Вселенной» |
| missions/AMAZONIA-1/anchors/2810/contact | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Гипер-корабль Когниума: «вы не одни». Культурный шок |
| missions/FREE-VENTURE-1 | PROPOSAL | PUBLIC_RECORD | реконсиляция A5 (имя есть у автора, паспорт — Claude) | First anarcho-capitalist ark (small, crowdfunded) |
| missions/FREE-VENTURE-1/arrival_year | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | arrival_year |
| missions/FREE-VENTURE-1/fate | PROPOSAL | PUBLIC_RECORD | fate.md / досье | fate |
| missions/FREE-VENTURE-1/launch_year | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | launch_year |
| missions/FREE-VENTURE-1/population | PROPOSAL | PUBLIC_RECORD | число из досье (реконсиляция C1: стартовые численности по S1) | population |
| missions/FREE-VENTURE-1/anchors/2405/arrival | PROPOSAL | PUBLIC_RECORD | реконсиляция A5: цель перенесена на Эпсилон Эридана | Эпсилон Эридана, где уже 190 лет правит Гармония; поселение в поясе |
| missions/FREE-VENTURE-1/anchors/2440/clan_fragmentation | PROPOSAL | PUBLIC_RECORD | событие миссии-предложения | Гражданская война модулей: «Пояс Раздора» |
| missions/DHARMA-2/arrival_year | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | arrival_year |
| missions/DHARMA-2/anchors/2490/sleeper_return | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Патриции просыпаются после 90 лет и пытаются вернуть порядок; синтез |
| missions/DHARMA-2/anchors/2523/arrival | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | HD 20794: Вторая Эллада уже здесь 140 лет |
| missions/FREE-WORLD-1/arrival_year | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2570 | arrival_year |
| missions/FREE-WORLD-1/anchors/2535/schism | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Проснувшиеся патриции требуют «чистой» анархии; 800 человек отстыковывают модуль «Exit» — судьба неизвестна |
| missions/FREE-WORLD-1/anchors/2574/arrival | PROPOSAL | PUBLIC_RECORD | пересчитано по каталогу и потолку скорости; авторское 2570 | GJ 581, рядом с Новой Европой |
| missions/FREE-WORLD-1/anchors/2862/leak | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Кланы покупают спецификации гипердвигателя и выкладывают их «для всех» |
| missions/CELESTIAL-DESTINY-2/arrival_year | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | arrival_year |
| missions/CELESTIAL-DESTINY-2/anchors/2742/arrival | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | HD 40307: внешние спутники; холодная война |
| missions/IRON-SHIELD-2/fate | UNKNOWN | AUTHOR_TRUTH | неизвестно даже автору | fate |
| missions/GAIA-2/fate | PROPOSAL | AUTHOR_TRUTH | скрытая правда (реконсиляция): живые «тихие» и пустой корабль | fate |
| missions/GAIA-2/anchors/2800/arrival | PROPOSAL | AUTHOR_TRUTH | реконсиляция B14: люди исчезли после посадки, сводки шлёт ИИ | TRAPPIST-1: изучение 10 лет, посадка, исчезновение людей; ИИ «Гомер-2» продолжает слать «Сад растёт» |
| missions/NOMAD-1/anchors/2490/return_timer | PROPOSAL | PUBLIC_RECORD | реконсиляция E6: таймер возврата (воспоминание автора, не найдено в заметках) | Срабатывает тайный таймер возврата (provisional); бунт пассажиров против инженеров |
| missions/NOMAD-1/anchors/2497/refused | PROPOSAL | PUBLIC_RECORD | реконсиляция E6 | Земля отказывает в стыковке: активы пассажиров арестованы; Номад уходит в медленную петлю Сол — Барнард |
| missions/FINAL-ARK-1/anchors/2765/flyby | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Демонстрационный прыжок гипер-корабля рядом с ковчегом («HYPERION обогнал их») |
| missions/DEEP-SEEK-1/fate | PROPOSAL | AUTHOR_TRUTH | скрытая правда (реконсиляция): уничтожен автономным роем CD-2 | fate |
| missions/DEEP-SEEK-1/launch_year | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | launch_year |
| missions/DEEP-SEEK-1/anchors/2615/institution | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | Совет Специалистов рядом с Советом Семей |
| missions/FAREWELL-1/fate | UNKNOWN | AUTHOR_TRUTH | неизвестно даже автору | fate |
| missions/HYPERION-1/anchors/2752/hyperion_2 | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | HYPERION-2: гиперпространство — среда с градиентами и течениями; без маяков прыжок — лотерея |
| missions/TRADERS-GUILD-1 | PROPOSAL | PUBLIC_RECORD | реконсиляция A6 (идея старой ветки) | Lalande colonies (New Hellas + New Mecca): first inter-colony sublight courier |
| missions/TRADERS-GUILD-1/arrival_year | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | arrival_year |
| missions/TRADERS-GUILD-1/fate | PROPOSAL | PUBLIC_RECORD | fate.md / досье | fate |
| missions/TRADERS-GUILD-1/launch_year | PROPOSAL | PUBLIC_RECORD | год в generation ships.md (A) | launch_year |
| missions/TRADERS-GUILD-1/population | PROPOSAL | PUBLIC_RECORD | число из досье (реконсиляция C1: стартовые численности по S1) | population |
| missions/TRADERS-GUILD-1/anchors/2512/arrival | PROPOSAL | PUBLIC_RECORD | не найдено в источниках | Тау Кита; капитан Лира Вэнс — легенда |
| missions/LAST-LIGHT-1 | PROPOSAL | AUTHOR_TRUTH | реконсиляция A8 (идея старой ветки, груз — Claude) | Earth: last official sublight embassy, to the Citadel |
| missions/LAST-LIGHT-1/arrival_year | PROPOSAL | PUBLIC_RECORD | год в fate.md | arrival_year |
| missions/LAST-LIGHT-1/fate | PROPOSAL | AUTHOR_TRUTH | скрытая правда (реконсиляция): груз: архив Хартий и верительные грамоты Земли | fate |
| missions/LAST-LIGHT-1/launch_year | PROPOSAL | PUBLIC_RECORD | год не найден в источниках миссии | launch_year |
| missions/LAST-LIGHT-1/population | PROPOSAL | PUBLIC_RECORD | число из досье (реконсиляция C1: стартовые численности по S1) | population |
| missions/LAST-LIGHT-1/anchors/2690/arrival | PROPOSAL | PUBLIC_RECORD | год в fate.md | Вольф 359: Цитадель задерживает корабль и груз |

## outposts (8)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| outposts/eps_indi_field | PROPOSAL | PUBLIC_RECORD | аванпост гипер-эпохи (реконсиляция) | Гелиевое поле Эпсилон Индейца |
| outposts/gj667_ubuntu_node | PROPOSAL | PUBLIC_RECORD | аванпост гипер-эпохи (реконсиляция) | Узел Убунту (ретранслятор у обломков) |
| outposts/gj892_kern | PROPOSAL | PUBLIC_RECORD | аванпост гипер-эпохи (реконсиляция) | Стратегические рудники Gliese 892 |
| outposts/hd10360_relay | PROPOSAL | PUBLIC_RECORD | аванпост гипер-эпохи (реконсиляция) | Реле HD 10360 |
| outposts/barnard_post | PROPOSAL | PUBLIC_RECORD | аванпост гипер-эпохи (реконсиляция) | Пост прослушивания у Барнарда |
| outposts/hd114783_claims | PROPOSAL | PUBLIC_RECORD | аванпост гипер-эпохи (реконсиляция) | Лагерь «Кларион» |
| outposts/leo83_golden_city | PROPOSAL | PUBLIC_RECORD | аванпост гипер-эпохи (реконсиляция) | «Золотой Город» |
| outposts/sigma2uma_expedition | PROPOSAL | PUBLIC_RECORD | аванпост гипер-эпохи (реконсиляция) | Экспедиция Sigma² UMa |

## relations (1)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| relations/lalande_hellas|lalande_mecca | PROPOSAL | PUBLIC_RECORD | величина обиды — Claude | Одна бедная система на двоих |

## unknowns (5)

| путь | статус | видимость | основание | что |
|---|---|---|---|---|
| unknowns/IRON-SHIELD-2.cause | UNKNOWN | AUTHOR_TRUTH | намеренно без ответа | что случилось с IRON-SHIELD-2 (3 000 человек, 2355) |
| unknowns/NIRVANA-1.fate | UNKNOWN | AUTHOR_TRUTH | намеренно без ответа | судьба и положение NIRVANA-1 |
| unknowns/UBUNTU-1.cause | UNKNOWN | AUTHOR_TRUTH | намеренно без ответа | причина гибели UBUNTU-1 («огни») |
| unknowns/FAREWELL-1.people | UNKNOWN | AUTHOR_TRUTH | намеренно без ответа | судьба людей FAREWELL-1 |
| unknowns/EARTH.jump_cores | UNKNOWN | AUTHOR_TRUTH | намеренно без ответа | умеет ли Земля 3126 делать собственные ядра прыжка (только маяк из памятника HYPERION-1) |

## settlements (153)

Все поселения и площадки в `canon/settlements.json` — PROPOSAL (на телах каталога).

