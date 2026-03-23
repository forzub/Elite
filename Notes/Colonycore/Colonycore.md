Да. Ниже сведу всё в **единую систему слоёв** и распишу каждый слой через JSON-поля так, чтобы это потом можно было превратить в тестовую программу и проверить, хватает ли модели.

Сразу зафиксирую принцип:

# Общая логика

У тебя есть не один “характер колонии”, а несколько слоёв:

1. **BaseCulture** — как это общество живёт в норме
2. **ThreatProfile** — чего оно боится
3. **NarrativeProfile** — как оно объясняет мир, власть, жертву, историю
4. **Constraints / Environment** — в каких объективных условиях оно живёт
5. **Pressures** — что на него сейчас давит
6. **ErgoState** — текущее рабочее состояние общества
7. **DerivedMetrics** — производные показатели, которые считаются, а не хранятся

---

# 1. Корневая сущность общества

Это не “колония” как вывеска, а **непрерывная социальная линия**.

```json
{
  "id": "soc_tau_ceti_belt_01",
  "kind": "societal_line",
  "current_name": "Silent Ring",
  "historical_names": [
    {
      "name": "New Plymouth Belt Colony",
      "from_year": 2236,
      "to_year": 2264
    },
    {
      "name": "Silent Ring",
      "from_year": 2264,
      "to_year": null
    }
  ],
  "origin": {
    "mission_id": "mission_mayflower_2",
    "origin_world": "Earth",
    "origin_year": 2088
  },
  "population": {
    "current_total": 21250,
    "founding_total": 25000,
    "population_groups": [
      {
        "id": "elite_patronage_stratum",
        "share_percent": 18
      },
      {
        "id": "contract_technical_stratum",
        "share_percent": 57
      },
      {
        "id": "penal_discarded_stratum",
        "share_percent": 25
      }
    ]
  },
  "base_culture": {},
  "threat_profile": {},
  "narrative_profile": {},
  "constraints": {},
  "active_pressures": [],
  "ergo_state": {},
  "metadata": {
    "notes": "",
    "tags": []
  }
}
```

---

# 2. Слой BaseCulture

Это **нормальный способ жить**, когда нет острого кризиса.
Не гарнизон. Не осада. Не ЧП. А повседневная норма.

## JSON-структура

```json
{
  "base_culture": {
    "lifestyle": {
      "tempo_discipline": 50,
      "schedule_rigidity": 50,
      "comfort_priority": 50,
      "leisure_value": 50,
      "social_warmth": 50,
      "expressiveness": 50,
      "formality": 50,
      "resource_thrift": 50,
      "aesthetic_drive": 50,
      "material_aspiration": 50
    },
    "social_order": {
      "cohesion": 50,
      "institutional_trust": 50,
      "external_trust": 50,
      "control_acceptance": 50,
      "risk_tolerance": 50,
      "innovation_drive": 50,
      "tradition_grip": 50,
      "future_confidence": 50,
      "status_competition": 50,
      "collective_purpose": 50
    },
    "stress_baseline": {
      "militarization": 20,
      "paranoia": 20,
      "hardship_tolerance": 50,
      "identity_rigidity": 50,
      "external_authority_acceptance": 50,
      "compromise_norm": 50
    },
    "expansion_baseline": {
      "security_buffer_drive": 50,
      "opportunity_expansion_drive": 50,
      "land_utilization_intensity": 50,
      "territorial_attachment": 50
    }
  }
}
```

---

## Что значат поля

### lifestyle

* `tempo_discipline` — насколько жизнь живёт по плотному темпу
* `schedule_rigidity` — насколько расписание считается обязательным
* `comfort_priority` — насколько комфорт и самочувствие важнее голой эффективности
* `leisure_value` — насколько отдых считается нормальной ценностью
* `social_warmth` — теплота и близость в повседневном общении
* `expressiveness` — эмоциональная открытость
* `formality` — степень социальной формальности
* `resource_thrift` — хозяйственность и склонность экономить
* `aesthetic_drive` — склонность вкладываться в красоту, стиль, символы среды
* `material_aspiration` — тяга к улучшению материального уровня и накоплению

### social_order

* `cohesion` — внутренняя сплочённость
* `institutional_trust` — доверие своим институтам
* `external_trust` — доверие внешним людям/сообществам
* `control_acceptance` — принятие правил и регламента
* `risk_tolerance` — терпимость к неопределённости
* `innovation_drive` — готовность пробовать новое
* `tradition_grip` — сила привычных форм и ритуалов
* `future_confidence` — вера в завтрашний день
* `status_competition` — насколько общество живёт через ранги, престиж, позицию
* `collective_purpose` — насколько сильно ощущение общего дела

### stress_baseline

* `militarization` — насколько сила и оружие нормальны даже без войны
* `paranoia` — базовое ожидание угрозы
* `hardship_tolerance` — терпимость к лишениям
* `identity_rigidity` — жёсткость самоопределения “кто мы”
* `external_authority_acceptance` — допустимость внешней власти
* `compromise_norm` — насколько компромисс считается нормальным, а не унизительным

### expansion_baseline

* `security_buffer_drive` — тяга к расширению ради безопасности
* `opportunity_expansion_drive` — тяга к расширению ради выгоды и роста
* `land_utilization_intensity` — склонность не просто держать территорию, а плотно её осваивать
* `territorial_attachment` — насколько тяжело “уйти с места”

---

# 3. Слой ThreatProfile

Это то, **чего общество боится сильнее всего**.
Не обязательно объективно. Важно, что оно считает это смертельно опасным.

## JSON-структура

```json
{
  "threat_profile": {
    "systemic_threats": {
      "system_collapse": 50,
      "loss_of_control": 50,
      "institutional_decay": 50,
      "lawlessness": 50,
      "internal_fragmentation": 50
    },
    "external_threats": {
      "external_enemy": 50,
      "foreign_rule": 50,
      "encirclement": 50,
      "cultural_erasure": 50
    },
    "unknown_threats": {
      "unknown_presence": 50,
      "unexplained_disaster": 50,
      "hidden_infiltration": 50,
      "cosmic_or_existential_unknown": 50
    },
    "material_threats": {
      "resource_loss": 50,
      "infrastructure_failure": 50,
      "supply_disruption": 50,
      "habitat_breach": 50,
      "environmental_instability": 50
    }
  }
}
```

---

## Что значат поля

* `system_collapse` — страх, что система развалится целиком

* `loss_of_control` — страх утраты управления

* `institutional_decay` — страх разложения институтов

* `lawlessness` — страх хаоса без правил

* `internal_fragmentation` — страх раскола своих

* `external_enemy` — страх внешнего врага

* `foreign_rule` — страх чужой власти

* `encirclement` — страх быть окружённым, зажатым

* `cultural_erasure` — страх растворения, потери себя

* `unknown_presence` — страх чего-то непонятного, скрытого

* `unexplained_disaster` — страх необъяснимой катастрофы

* `hidden_infiltration` — страх незаметного проникновения врага/чужого

* `cosmic_or_existential_unknown` — страх непознаваемого и запредельного

* `resource_loss` — страх нехватки ресурсов

* `infrastructure_failure` — страх отказа жизненно важных систем

* `supply_disruption` — страх срыва снабжения

* `habitat_breach` — страх разгерметизации, утраты убежища

* `environmental_instability` — страх нестабильной среды

---

# 4. Слой NarrativeProfile

Это **система смыслов**, через которую общество объясняет:

* жертву,
* власть,
* смерть,
* историю,
* миссию,
* трагедии.

## JSON-структура

```json
{
  "narrative_profile": {
    "sacralization": {
      "leader_sacralization": 50,
      "martyrdom_value": 50,
      "holy_mission": 50,
      "transcendence_belief": 50
    },
    "historical_consciousness": {
      "historical_memory": 50,
      "ancestral_continuity": 50,
      "myth_creation": 50,
      "golden_age_orientation": 50
    },
    "world_explanation": {
      "fatalism": 50,
      "providentialism": 50,
      "enemy_personalization": 50,
      "conspiracy_receptivity": 50
    },
    "legitimacy_model": {
      "rule_by_contract": 50,
      "rule_by_sacred_order": 50,
      "rule_by_strength": 50,
      "rule_by_merit": 50,
      "rule_by_lineage": 50
    },
    "identity_narrative": {
      "chosen_people_sense": 50,
      "frontier_destiny": 50,
      "siege_identity": 50,
      "civilizing_mission": 50,
      "purity_preservation": 50
    }
  }
}
```

---

## Что значат поля

### sacralization

* `leader_sacralization` — склонность сакрализовать вождя
* `martyrdom_value` — ценность жертвы ради дела
* `holy_mission` — вера в священную миссию
* `transcendence_belief` — вера, что смысл и порядок выходят за рамки материального

### historical_consciousness

* `historical_memory` — длинная память общества
* `ancestral_continuity` — чувство связи с предками
* `myth_creation` — способность быстро превращать события в миф
* `golden_age_orientation` — ориентация на “золотое прошлое”

### world_explanation

* `fatalism` — склонность считать многое предрешённым
* `providentialism` — вера, что ход истории имеет высший смысл
* `enemy_personalization` — склонность видеть за угрозами конкретного врага
* `conspiracy_receptivity` — готовность принять скрытые объяснения

### legitimacy_model

Это не одно поле, а конкурирующие модели легитимности:

* `rule_by_contract` — власть через договор
* `rule_by_sacred_order` — власть через сакральный порядок
* `rule_by_strength` — власть через силу
* `rule_by_merit` — власть через компетентность
* `rule_by_lineage` — власть через происхождение

### identity_narrative

* `chosen_people_sense` — чувство исключительности
* `frontier_destiny` — вера в предназначение осваивать и идти дальше
* `siege_identity` — ощущение себя как осаждённой крепости
* `civilizing_mission` — вера, что они несут порядок другим
* `purity_preservation` — установка на сохранение “чистоты” себя

---

# 5. Слой Constraints / Environment

Это **объективные условия**, а не психология.

## JSON-структура

```json
{
  "constraints": {
    "physical_environment": {
      "habitat_type": "asteroid_belt",
      "gravity_regime": "microgravity",
      "climate_harshness": 70,
      "environmental_predictability": 60,
      "space_isolation": 80,
      "mobility": 20
    },
    "resource_conditions": {
      "resource_scarcity": 45,
      "resource_volatility": 60,
      "food_security": 55,
      "energy_security": 70,
      "industrial_self_sufficiency": 65
    },
    "strategic_conditions": {
      "geostrategic_vulnerability": 75,
      "border_depth": 20,
      "buffer_space_available": 35,
      "exit_options": 5,
      "external_contact_density": 15
    },
    "social_structure": {
      "class_stratification": 80,
      "population_density": 35,
      "demographic_pressure": 40,
      "governance_centralization": 70,
      "enforcement_capacity": 65
    }
  }
}
```

---

## Что значат поля

### physical_environment

* `habitat_type` — планета, станция, корабль, астероид, пояс и т.д.
* `gravity_regime` — нормальная гравитация, низкая, микрогравитация
* `climate_harshness` — суровость среды
* `environmental_predictability` — предсказуемость условий
* `space_isolation` — изоляция в пространственном смысле
* `mobility` — насколько легко перемещаться

### resource_conditions

* `resource_scarcity` — дефицит ресурсов
* `resource_volatility` — нестабильность доступа
* `food_security` — надёжность продовольствия
* `energy_security` — надёжность энергии
* `industrial_self_sufficiency` — способность производить нужное самим

### strategic_conditions

* `geostrategic_vulnerability` — насколько уязвимо положение
* `border_depth` — глубина пространства перед ядром
* `buffer_space_available` — можно ли отодвигать угрозу буфером
* `exit_options` — можно ли уйти/эвакуироваться
* `external_contact_density` — насколько плотны внешние связи

### social_structure

* `class_stratification` — расслоение
* `population_density` — плотность населения
* `demographic_pressure` — давление численности на систему
* `governance_centralization` — степень централизации
* `enforcement_capacity` — способность реально заставлять соблюдать правила

---

# 6. Слой Pressures

Это **активные давления**, действующие сейчас или в заданный период.

## JSON-структура одного pressure

```json
{
  "id": "pressure_unknown_belt_threat",
  "type": "existential_unknown",
  "start_year": 2253,
  "end_year": 2370,
  "intensity": 75,
  "source_event_id": "evt_branch_expedition_lost",
  "description": "После потери экспедиции общество живёт в ожидании повторения неизвестной угрозы.",
  "effects": {
    "ergo_modifiers": {
      "paranoia": 18,
      "militarization": 12,
      "external_trust": -20,
      "control_acceptance": 10,
      "collective_purpose": 6
    },
    "culture_drift_bias": {
      "siege_identity": 8,
      "hardship_tolerance": 5,
      "compromise_norm": -7
    },
    "narrative_activation": {
      "myth_creation": 10,
      "enemy_personalization": 14,
      "conspiracy_receptivity": 6
    }
  },
  "tags": [
    "trauma",
    "unknown_enemy",
    "long_duration"
  ]
}
```

---

## Типовой контейнер active_pressures

```json
{
  "active_pressures": [
    {
      "id": "pressure_unknown_belt_threat",
      "type": "existential_unknown",
      "start_year": 2253,
      "end_year": 2370,
      "intensity": 75,
      "source_event_id": "evt_branch_expedition_lost",
      "description": "",
      "effects": {
        "ergo_modifiers": {},
        "culture_drift_bias": {},
        "narrative_activation": {}
      },
      "tags": []
    }
  ]
}
```

---

## Типы pressure, которые стоит сразу предусмотреть

```json
{
  "pressure_types": [
    "stable_isolation",
    "volatile_isolation",
    "resource_scarcity",
    "resource_volatility",
    "existential_unknown",
    "external_military_threat",
    "internal_fragmentation",
    "elite_capture",
    "mission_fatigue",
    "generational_drift",
    "infrastructure_decay",
    "rapid_opportunity_window",
    "messianic_mobilization",
    "frontier_expansion_pull"
  ]
}
```

---

# 7. Слой ErgoState

Это **текущее рабочее состояние общества** на выбранный год.

По структуре оно должно быть похоже на BaseCulture, чтобы было удобно сравнивать “норма” и “факт”.

## JSON-структура

```json
{
  "ergo_state": {
    "lifestyle": {
      "tempo_discipline": 62,
      "schedule_rigidity": 58,
      "comfort_priority": 44,
      "leisure_value": 39,
      "social_warmth": 57,
      "expressiveness": 46,
      "formality": 63,
      "resource_thrift": 71,
      "aesthetic_drive": 28,
      "material_aspiration": 42
    },
    "social_order": {
      "cohesion": 74,
      "institutional_trust": 52,
      "external_trust": 14,
      "control_acceptance": 77,
      "risk_tolerance": 33,
      "innovation_drive": 48,
      "tradition_grip": 69,
      "future_confidence": 29,
      "status_competition": 41,
      "collective_purpose": 76
    },
    "stress_response": {
      "militarization": 83,
      "paranoia": 91,
      "hardship_tolerance": 79,
      "identity_rigidity": 81,
      "external_authority_acceptance": 9,
      "compromise_norm": 18
    },
    "expansion_behavior": {
      "security_buffer_drive": 88,
      "opportunity_expansion_drive": 23,
      "land_utilization_intensity": 46,
      "territorial_attachment": 84
    }
  }
}
```

---

## Смысл слоя ErgoState

Это ответ на вопрос:

> Какими они являются сейчас, на этом году, под действием среды, давлений, мифов и истории?

---

# 8. DerivedMetrics

Это **не хранимые**, а считаемые показатели. Но для тестовой программы полезно выписать их явно.

## JSON-шаблон

```json
{
  "derived_metrics": {
    "compromise_flexibility": 0,
    "resistance_intensity": 0,
    "fragmentation_risk": 0,
    "security_expansion_pressure": 0,
    "opportunity_expansion_pressure": 0,
    "authoritarian_drift": 0,
    "xenophobia_pressure": 0,
    "social_fatigue": 0,
    "civilian_quality_of_life": 0,
    "narrative_radicalization": 0
  }
}
```

---

## Что значат поля

* `compromise_flexibility` — насколько реально возможен компромисс
* `resistance_intensity` — насколько общество будет драться до конца
* `fragmentation_risk` — риск внутреннего раскола
* `security_expansion_pressure` — давление расширения ради буфера
* `opportunity_expansion_pressure` — давление расширения ради выгоды
* `authoritarian_drift` — тенденция к ужесточению режима
* `xenophobia_pressure` — тенденция к враждебности к чужим
* `social_fatigue` — накопленная усталость общества
* `civilian_quality_of_life` — бытовое качество жизни
* `narrative_radicalization` — степень радикализации смысловой картины мира

---

# 9. История и события

Ты просил именно слои, но без событий эта система не оживёт. Поэтому сразу зафиксирую и шаблон события.

## JSON-структура события

```json
{
  "id": "evt_branch_expedition_lost",
  "year": 2253,
  "type": "expedition_lost",
  "title": "Исчезновение поясовой экспедиции",
  "summary": "Экспедиция в дальний сектор пояса перестала выходить на связь после обрывков сигнала с криками и стрельбой.",
  "location": {
    "system_id": "tau_ceti",
    "zone_id": "belt_sector_outer_4"
  },
  "affected_entities": [
    "soc_tau_ceti_belt_01"
  ],
  "demographic_effects": {
    "population_loss_absolute": 3750,
    "population_loss_percent": 15
  },
  "generated_pressures": [
    "pressure_unknown_belt_threat",
    "pressure_collective_trauma",
    "pressure_security_mobilization"
  ],
  "notes": ""
}
```

---

# 10. Полная каноническая схема общества

Вот как это всё выглядит в одном объекте.

```json
{
  "id": "soc_tau_ceti_belt_01",
  "kind": "societal_line",
  "current_name": "Silent Ring",
  "historical_names": [],
  "origin": {},
  "population": {},
  "base_culture": {},
  "threat_profile": {},
  "narrative_profile": {},
  "constraints": {},
  "active_pressures": [],
  "ergo_state": {},
  "metadata": {
    "notes": "",
    "tags": []
  }
}
```

---

# 11. Что надо проверить в тестовой программе

Когда будешь делать тестовую программу, ей нужно проверять не только синтаксис, но и **структурную пригодность модели**.

## Проверки уровня 1 — валидность

* все поля существуют
* все значения числовых параметров лежат в диапазоне 0–100
* обязательные блоки не пустые

## Проверки уровня 2 — логическая непротиворечивость

Например:

* если `external_authority_acceptance` очень низкий, а `compromise_norm` очень высокий — это подозрительно
* если `paranoia` очень высокая, а `unknown_threat` и `external_enemy` очень низкие — нужно объяснение
* если `security_buffer_drive` высокий, а `geostrategic_vulnerability` низкая — возможно, это не ошибка, но требует причины
* если `comfort_priority` и `hardship_tolerance` обе очень высокие — может быть нормой, но это уже интересный профиль

## Проверки уровня 3 — чувствительность

Надо смотреть:

* не дублируют ли поля друг друга
* не слишком ли много похожих осей
* можно ли объяснить общество без трёх разных полей, означающих одно и то же

---

# 12. Что, скорее всего, потом придётся уточнить

Вот поля, которые почти наверняка потребуют калибровки после тестов:

* `formality`
* `expressiveness`
* `material_aspiration`
* `aesthetic_drive`
* `collective_purpose`
* `identity_rigidity`
* `compromise_norm`
* `chosen_people_sense`
* `civilizing_mission`

Они полезны, но их легче всего сделать расплывчатыми. Именно их потом тестовая программа должна “прожарить”.

---

# 13. Мой практический совет по следующему шагу

Не пытайся сразу набить этим все колонии.

Сначала сделай **одну эталонную**:

* стартовая миссия,
* прибытие,
* решение остаться,
* потеря 15%,
* 100 лет дрейфа.

И уже на ней смотри:

* хватает ли полей,
* нет ли дублей,
* не забыли ли какой-то важный фактор.

---

