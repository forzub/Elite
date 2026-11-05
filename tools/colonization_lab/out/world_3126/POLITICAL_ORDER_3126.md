# Политический порядок 3126 (PROPOSAL_PENDING_AUTHOR_REVIEW)

Сгенерировано `tools/colonization_lab/political.py` из `canon/political_order.json`.

```text
actors_3126 (21): cyg61_kern, delta_pav_freekarma, delta_pav_karma, eps_eri_harmony, eps_eri_strife, gj581_europa, gj581_freeclans, gj832_tianhe, gj876_next, hd114783_hellas3, hd20794_hellas2, hd20794_newindia, hd40307_maland, hd69830_verde, lalande_hellas, lalande_mecca, leo83_zion, proxima_outpost, sol_authority, tau_ceti_cognium, wolf359_citadel
economic_units (23): tau_ceti_cognium, proxima_outpost, gj832_tianhe, gj832_gaia_zone, lalande_hellas, lalande_mecca, eps_eri_harmony, eps_eri_strife, wolf359_citadel, gj581_europa, gj581_freeclans, hd20794_hellas2, hd20794_newindia, hd40307_maland, hd40307_firmament, hd69830_verde, delta_pav_karma, delta_pav_freekarma, gj876_next, cyg61_kern, hd114783_hellas3, leo83_zion, sol_authority
provinces (1): hd40307_firmament -> eps_eri_harmony
special_zones (1): gj832_gaia_zone (quarantine_exclusion)
bloc cognium_pool [alliance, integration high] members 4: cyg61_kern, delta_pav_freekarma, hd20794_hellas2, tau_ceti_cognium; units: cyg61_kern, delta_pav_freekarma, hd20794_hellas2, tau_ceti_cognium
bloc heavenly_pact [alliance, integration very_high] members 2: eps_eri_harmony, gj832_tianhe; units: eps_eri_harmony, gj832_tianhe, hd40307_firmament
bloc lalande_league [alliance, integration high_core_low_associate] members 3: gj581_europa, hd114783_hellas3 (associate), lalande_hellas; units: gj581_europa, hd114783_hellas3, lalande_hellas
bloc southern_circle [alliance, integration medium] members 3: hd20794_newindia, hd40307_maland, lalande_mecca; units: hd20794_newindia, hd40307_maland, lalande_mecca
bloc citadel_compact [alliance, integration medium] members 2: proxima_outpost, wolf359_citadel; units: proxima_outpost, wolf359_citadel
bloc free_belts [space, integration low_formal_high_informal] members 2: eps_eri_strife, gj581_freeclans; units: eps_eri_strife, gj581_freeclans
bloc sol [pole, integration n/a] members 1: sol_authority; units: sol_authority
nonaligned (4): delta_pav_karma (ideological_isolationist), gj876_next (strategic_multivector), hd69830_verde (resource_neutral), leo83_zion (newcomer_religious)
  actor cyg61_kern: bloc cognium_pool, units ['cyg61_kern'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor delta_pav_freekarma: bloc cognium_pool, units ['delta_pav_freekarma'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor delta_pav_karma: bloc NONALIGNED:ideological_isolationist, units ['delta_pav_karma'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor eps_eri_harmony: bloc heavenly_pact, units ['eps_eri_harmony', 'hd40307_firmament'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor eps_eri_strife: bloc free_belts, units ['eps_eri_strife'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor gj581_europa: bloc lalande_league, units ['gj581_europa'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor gj581_freeclans: bloc free_belts, units ['gj581_freeclans'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor gj832_tianhe: bloc heavenly_pact, units ['gj832_tianhe'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor gj876_next: bloc NONALIGNED:strategic_multivector, units ['gj876_next'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor hd114783_hellas3: bloc lalande_league, units ['hd114783_hellas3'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor hd20794_hellas2: bloc cognium_pool, units ['hd20794_hellas2'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor hd20794_newindia: bloc southern_circle, units ['hd20794_newindia'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor hd40307_maland: bloc southern_circle, units ['hd40307_maland'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor hd69830_verde: bloc NONALIGNED:resource_neutral, units ['hd69830_verde'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor lalande_hellas: bloc lalande_league, units ['lalande_hellas'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor lalande_mecca: bloc southern_circle, units ['lalande_mecca'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor leo83_zion: bloc NONALIGNED:newcomer_religious, units ['leo83_zion'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor proxima_outpost: bloc citadel_compact, units ['proxima_outpost'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor sol_authority: bloc sol, units ['sol_authority'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor tau_ceti_cognium: bloc cognium_pool, units ['tau_ceti_cognium'], status PROPOSAL_PENDING_AUTHOR_REVIEW
  actor wolf359_citadel: bloc citadel_compact, units ['wolf359_citadel'], status PROPOSAL_PENDING_AUTHOR_REVIEW
```

## Доступ между столицами блоков и неприсоединившимися (продавец → покупатель)

Шкала: 0 открыто; 1 по лицензии; 2 ограничено; 3 санкции / косвенно; 4 закрыто. Порядок категорий: civilian_goods, industrial, dual_use, military, strategic, people_finance_data.

| продавец \ покупатель | tau_ceti_cognium | eps_eri_harmony | lalande_hellas | hd20794_newindia | wolf359_citadel | eps_eri_strife | sol_authority | delta_pav_karma | gj876_next | hd69830_verde | leo83_zion | gj832_gaia_zone |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| tau_ceti_cognium | — | 011222 | 001220 | 011221 | 013443 | 001241 | 001222 | 234444 | 001221 | 001221 | 001242 | 344442 |
| eps_eri_harmony | 012242 | — | 012240 | 012242 | 012243 | 023443 | 012242 | 234444 | 001241 | 012242 | 012242 | 344442 |
| lalande_hellas | 001230 | 011230 | — | 011230 | 012240 | 001230 | 001230 | 234444 | 001230 | 001230 | 001230 | 344442 |
| hd20794_newindia | 012241 | 012242 | 012240 | — | 012243 | 012241 | 012242 | 234444 | 001241 | 012241 | 012242 | 344442 |
| wolf359_citadel | 013443 | 013443 | 012440 | 013443 | — | 013443 | 013443 | 234444 | 001441 | 013443 | 013443 | 344443 |
| eps_eri_strife | 013332 | 023443 | 012330 | 013332 | 013343 | — | 013332 | 234444 | 001331 | 013332 | 013332 | 344442 |
| sol_authority | 012241 | 012242 | 012240 | 012241 | 012243 | 012241 | — | 234444 | 001241 | 012241 | 012242 | 344442 |
| delta_pav_karma | 223444 | 223444 | 223444 | 223444 | 223444 | 223444 | 223444 | — | 223444 | 223444 | 223444 | 344444 |
| gj876_next | 001231 | 001231 | 001230 | 001231 | 001241 | 001231 | 001231 | 234444 | — | 001231 | 001231 | 344442 |
| hd69830_verde | 001241 | 011242 | 001240 | 011241 | 012243 | 001241 | 001242 | 234444 | 001241 | — | 001242 | 344442 |
| leo83_zion | 012342 | 012342 | 012340 | 012342 | 012343 | 012342 | 012342 | 234444 | 001341 | 012342 | — | 344442 |
| gj832_gaia_zone | 444442 | 444442 | 444442 | 444442 | 444443 | 444442 | 444442 | 444444 | 444442 | 444442 | 444442 | — |

## Проверки

- OK every_unit_mapped_once: missing [], extra []
- OK unit_in_at_most_one_actor: 22
- OK actor_seat_is_own_unit: 
- OK actor_bloc_known: 
- OK nonaligned_have_profile_and_type: 
- OK no_autarkies_bloc: 
- OK zones_not_actors: 
- OK provinces_inherit_bloc: 
- OK profiles_cover_all_categories: 
- OK exceptions_reference_actors: 
- OK sanctions_derived_from_relations_anchor: 8 measures
- OK citadel_strategic_closed_outside_pact: 
- OK mecca_strategic_for_southern_circle: 
- OK cognium_cores_reach_sol_league_pact: 
- OK kern_finance_crosses_blocs: Kern finance <= licensed with everyone except Karma (isolation), Third Hellas (claims freeze), zones
- OK agora_more_open_than_league_peer: 
- OK firmament_local_conflict_only: 
- OK hellas3_less_integrated_than_core: 
- OK southern_circle_less_integrated_than_pact: 
- OK zone_closed_to_ordinary_trade: 
- OK nonaligned_not_mutual_allies: 
- OK cognium_cores_only_to_licence_holders: ok
- OK no_cognium_cores_for_refusers_and_copiers: proxima_outpost,eps_eri_strife,gj581_freeclans,delta_pav_karma
- OK own_core_makers_not_licence_buyers: tau_ceti_cognium,lalande_mecca,wolf359_citadel
- OK controlled_goods_not_military: only the weapons group is military; narcotics = civilian_goods + legality layer
- OK transit_never_strategic: 
- OK finance_bridge_no_strategic_cap: 
- OK kern_cannot_sell_strategic_to_citadel: 
- OK providence_does_not_lift_sanctions: 
- OK citadel_civilian_open_strategic_closed: 
- OK every_element_has_player_role: Chekhov's gun: all blocs, non-aligned, units, exceptions, licences, sanctions
- OK sanctions_realistically_leaky: civilian / industrial / dual-use never fully closed without a stated reason; every measure has an evasion route
- OK no_pool_wide_embargo_on_karma: only Free Karma's own measure; pool coordinates strategic / dual-use / IP, civilian trade is national
- OK thebaid_kern_interim_open_no_sanctions: 
- OK newcomers_not_licensed_yet: 
- OK free_belts_military_only_grey: 
