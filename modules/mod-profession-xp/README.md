# mod-profession-xp

Character experience for gathering and crafting, as the original CoA granted it.

Mining, herbing, skinning, prospecting, milling and crafting each award ordinary character
experience on top of the usual skill-up roll. Restores issues #368, #1410, #1966 and #4266.

## How much

The award is what a mob of your own level gives (`Acore::XP::BaseGain`, the core's own formula),
scaled by the **colour of the node or recipe against your skill** — orange, yellow, green, and
grey which always awards nothing. The core already computes those thresholds; the module reads
them from the hook rather than recomputing them.

Colour alone is enough to stop a node being farmed, because skill only ever rises: an orange node
becomes yellow, then green, then grey, and stops paying.

Scaling the award down by the node's tier against your level was tried and removed. The two
measures disagree — colour is relative to skill and tier to level — so a level 22 who had just
learned mining was shown an orange copper vein that the tier rule called trivial and paid nothing
for, which reads as the feature being broken.

No canonical formula survived from live CoA and the player reports disagree, so the amount is a
preset in `professionxp.conf` rather than a constant:

| Preset | Orange pays | Reading |
| --- | --- | --- |
| `flavour` | 0.15 of a same-level kill | a small bonus, levelling unchanged |
| `balanced` | 0.50 | **default** — matches #1966 and #368 |
| `realpath` | 1.00 | matches #1410 — one node equals one mob kill |
| `custom` | your own three values | |

Yellow pays 60% of orange and green 30%, unless `custom` breaks the ratio.

## When

On every gather and every craft, not only the ones that raise the skill — #368 reports
experience for gathering a herb, which frequently does not skill up. That also covers what
#4266 asks for, since a craft that produces a skill up is paid like any other.

## Notes

- All values are read at startup and on `.reload config`, so the realm can be retuned without a
  restart or a rebuild.
- `Profession.XP.FollowPlayerRate` (on by default) makes this experience obey the rate a player
  chose with `.xp`, by raising `OnPlayerGiveXP` the way the core's own callers do. `Player::GiveXP`
  does not raise that hook itself, so without this a player on `.xp 5` would earn five times as
  much from a mob as from a node.
- Lockpicking is off by default: a lock can be picked repeatedly at no cost.
- Fishing is not covered. `Player::UpdateFishingSkill` uses a probability ladder rather than the
  colour thresholds, so it would need a rule of its own.
