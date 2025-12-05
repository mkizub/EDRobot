//
// Created by mkizub on 11.08.2025.
//

#pragma once

#ifndef EDROBOT_SHIPSTATS_H
#define EDROBOT_SHIPSTATS_H

namespace eddb {

class ShipStats;
class ShipSlot;

const double DNaN = std::numeric_limits<double>::quiet_NaN();

enum class ShipSlotGroup {
    UNUSED, SHIP, HARDPOINT, UTILITY, COMPONENT, MILITARY, INTERNAL
};
enum class ShipSlotName {
    Hull, Hatch, CargoHatch, ShipCockpit, // SHIP group
    Armour, PowerPlant, MainEngines, FrameShiftDrive, LifeSupport, PowerDistributor, Radar, FuelTank,
    PlanetaryApproachSuite, DataLinkScanner, CodexScanner, DiscoveryScanner, ColonisationSuite
};
enum class Attr {
                    //	{ attr:'stype' },
                    //	{ attr:'mtype' },
                    //	{ attr:'name' },
                    //{ attr:'cost',       hidden:1,                        abbr:'Cost', name:'Cost',                 unit:'Cr',   bad:1, min:0,          step:1, default:0, scale:0,             desc:'Purchase cost (in credits)' }, // *
                    //	{ attr:'retail',                                                   name:'Retail Price',         unit:'Cr',   bad:1, min:0,          step:1, default:0, scale:0,             desc:'Total purchase cost (in credits) including the default loadout' }, // ship
                    //{ attr:'faction',    hidden:1,                        abbr:'Fac',  name:'Faction',        values:['','Alliance','Empire','Federation'],     default:'',                     desc:'Faction membership required to purchase' }, // ship
                    //{ attr:'rank',       hidden:1,                        abbr:'Rank', name:'Faction Rank',                      bad:1, min:0,          step:1, default:0, scale:0,             desc:'Faction rank level required to purchase' }, // ship
                    //{ attr:'crew',                                        abbr:'Crew', name:'Crew Seats',                               min:1,          step:1, default:1, scale:0,             desc:'Number of seats for multi-crew' }, // ship
                    //	{ attr:'class',      fdattr:'Size',                   abbr:'Cls',  name:'Class',                                    min:0, max:  8, step:1, default:0, scale:0,             desc:'Size class (0-8)' }, // *
                    //	{ attr:'rating',     fdattr:'Class',                  abbr:'Rtg',  name:'Rating',         values:['','A','B','C','D','E','F','G','H','I'],  default:'',                     desc:'Performance rating (A-I)' },  // *
    topspd,         //{ attr:'topspd',                                      abbr:'Spd',  name:'Top Speed',            unit:'M/s',         min:0,                  default:0, scale:0,             desc:'Maximum thruster speed with outfitting mass equal to thruster optimal mass' }, // ship
                    //{ attr:'bstspd',                                      abbr:'Bst',  name:'Boost Speed',          unit:'M/s',         min:0,                  default:0, scale:0,             desc:'Maximum boost speed with outfitting mass equal to thruster optimal mass' }, // ship
    minthrust,      //{ attr:'minthrust',                                   abbr:'NThr', name:'Minimum Thrust',       unit:'%',           min:0, max:100,         default:0, scale:1,             desc:'Thruster speed modifier with 0 ENG pips' }, // ship
                    //{ attr:'boostcost',                                   abbr:'BstC', name:'Boost Cost',           unit:'MW',   bad:1, min:0,                  default:0, scale:0,             desc:'Engines capacitor draw for engine boost (in megawatts per boost)' }, // ship
                    //{ attr:'boostint',                                    abbr:'BstI', name:'Boost Interval',       unit:'s',    bad:1, min:0,                  default:0, scale:0,             desc:'Minimum time between engine boosts (in seconds)' }, // ship
    fwdacc,         //{ attr:'fwdacc',                                      abbr:'Acl',  name:'Forward acceleration', unit:'M/s',         min:0,                  default:0, scale:0,             desc:'Forward acceleration with outfitting mass equal to thruster optimal mass' }, // ship
    revacc,         //{ attr:'revacc',                                      abbr:'Dcl',  name:'Reverse acceleration', unit:'M/s',         min:0,                  default:0, scale:0,             desc:'Reverse acceleration with outfitting mass equal to thruster optimal mass' }, // ship
                    //{ attr:'mnv',                                         abbr:'Mnv',  name:'Manoeuvrability',                          min:0, max: 10, step:1, default:0, scale:0,             desc:'Manoeuvrability rating (out of 10)' }, // ship
    pitch,          //{ attr:'pitch',                                       abbr:'Pch',  name:'Pitch Speed',          unit:'&deg;/s',     min:0,                  default:0, scale:2,             desc:'Base pitch speed (in degrees per second) with outfitting mass equal to thruster optimal mass' }, // ship
    yaw,            //{ attr:'yaw',                                         abbr:'Yaw',  name:'Yaw Speed',            unit:'&deg;/s',     min:0,                  default:0, scale:2,             desc:'Base yaw speed (in degrees per second) with outfitting mass equal to thruster optimal mass' }, // ship
    roll,           //{ attr:'roll',                                        abbr:'Rol',  name:'Roll Speed',           unit:'&deg;/s',     min:0,                  default:0, scale:2,             desc:'Base roll speed (in degrees per second) with outfitting mass equal to thruster optimal mass' }, // ship
    minpitch,       //{ attr:'minpitch',                                    abbr:'NPch', name:'Min Pitch Speed',      unit:'&deg;/s',     min:0,                  default:'pitch', scale:2,       desc:'Minimum pitch speed (in degrees per second) with outfitting mass equal to thruster optimal mass and 0 ENG pips' }, // ship
    minyaw,         //{ attr:'minyaw',                                      abbr:'NYaw', name:'Min Yaw Speed',        unit:'&deg;/s',     min:0,                  default:'yaw',   scale:2,       desc:'Minimum yaw speed (in degrees per second) with outfitting mass equal to thruster optimal mass and 0 ENG pips' }, // ship
    minroll,        //{ attr:'minroll',                                     abbr:'NRol', name:'Min Roll Speed',       unit:'&deg;/s',     min:0,                  default:'roll',  scale:2,       desc:'Minimum roll speed (in degrees per second) with outfitting mass equal to thruster optimal mass and 0 ENG pips' }, // ship
                    //{ attr:'shields',                                     abbr:'Shd',  name:'Shields',                                  min:0,          step:1, default:0, scale:0,             desc:'Base shield strength (modified by the shield generator module)' }, // ship
                    //{ attr:'armour',                                      abbr:'Arm',  name:'Armour',                                   min:0,          step:1, default:0, scale:0,             desc:'Base armour strength (modified by the bulkhead module)' }, // ship
                    //{ attr:'hardness',                                    abbr:'Hrd',  name:'Armour Hardness',                          min:0,          step:1, default:0, scale:0,             desc:'Armour hardness rating (compare to weapon armour pierce)' }, // ship
                    //{ attr:'heatcap',                                     abbr:'HCap', name:'Heat Capacity',                            min:0,          step:1, default:0, scale:0,             desc:'Nominal heat capacity' }, // ship
                    //{ attr:'heatdismin',                                  abbr:'NHDs', name:'Min Heat Dissipation',                     min:0,                  default:0, scale:2,             desc:'Minimum heat dissipation level' }, // ship
                    //{ attr:'heatdismax',                                  abbr:'XHDs', name:'Max Heat Dissipation',                     min:0,                  default:0, scale:2,             desc:'Maximum heat dissipation rate' }, // ship
                    //{ attr:'mount',                                       abbr:'Mnt',  name:'Mount',          values:['F','G','T'],                             default:'',                     desc:'Mount type (fixed/gimballed/turreted)' }, // h*
                    //{ attr:'missile',                                     abbr:'Msl',  name:'Missile Type',   values:['D','S'],                                 default:'',                     desc:'Missile type (dumbfire/seeking)' }, // h*
                    //{ attr:'mass',       fdattr:'Mass',                   abbr:'Mass', name:'Mass',                 unit:'T',    bad:1, min:0,                  default:0, scale:2,             desc:'Mass (in tons)' }, // *
                    //{ attr:'masslock',                                    abbr:'MLF',  name:'Mass Lock',                                min:0,          step:1, default:0, scale:0,             desc:'Mass lock factor' }, // ship
                    //{ attr:'fuelcost',                                    abbr:'FCst', name:'Fuel Cost',            unit:'Cr/T', bad:1, min:0,          step:1, default:0, scale:0,             desc:'Cost of fuel (in credits per ton)' }, // ship
                    //{ attr:'fuelreserve',                                 abbr:'FRes', name:'Fuel Reserve',         unit:'T',           min:0,                  default:0, scale:2,             desc:'Reserve fuel tank size (in tons)' }, // ship
                    //{ attr:'integ',      fdattr:'Integrity',              abbr:'Int',  name:'Integrity',                                min:0,                  default:0, scale:0,             desc:'Structural integrity' }, // *
                    //{ attr:'pwrdraw',    fdattr:'PowerDraw',              abbr:'PwD',  name:'Power Draw',           unit:'MW',   bad:1, min:0,                  default:0, scale:2,             desc:'Power draw (in megawatts)' }, // *
                    //{ attr:'boottime',   fdattr:'BootTime',               abbr:'Boot', name:'Boot Time',            unit:'s',    bad:1, min:0,          time:1, default:0, scale:0,             desc:'Time to reboot or bring online (in seconds)' }, // *
                    //{ attr:'spinup',     fdattr:'ShieldBankSpinUp',       abbr:'Spin', name:'Spin Up Time',         unit:'s',    bad:1, min:0,          time:1, default:0, scale:0,             desc:'Time to spin up before starting shield reinforcement (in seconds)' }, // iscb
                    //{ attr:'scbdur',     fdattr:'ShieldBankDuration',     abbr:'Dur',  name:'Duration',             unit:'s',           min:0,          time:1, default:0, scale:0,             desc:'Duration of shield reinforcement (in seconds)' }, // iscb
                    //{ attr:'shieldrnfps',fdattr:'ShieldBankReinforcement',abbr:'ShR',  name:'Shield Reinforcement', unit:'/s',          min:0,                  default:0, scale:1,             desc:'Shield reinforcement (in units per second)' }, // iscb
                    //{ attr:'scbheat',    fdattr:'ShieldBankHeat',         abbr:'ThL',  name:'Thermal Load',                      bad:1, min:0,                  default:0, scale:1,             desc:'Waste heat generated per use' }, // iscb
                    //{ attr:'dps',        fdattr:'DamagePerSecond',        abbr:'DPS',  name:'Damage per Second',    unit:'/s',          min:0,                  default:0, scale:2,             desc:'Raw damage per second, not including reload time' }, // h*,upd
                    //{ attr:'sdps',                                        abbr:'SDPS', name:'Sustained DPS',        unit:'/s',          min:0,                  default:0, scale:2,             desc:'Sustained damage per second, including reload time' },
                    //{ attr:'damage',     fdattr:'Damage',                 abbr:'Dmg',  name:'Damage',                                   min:0,                  default:0, scale:1,             desc:'Raw damage per shot, or per second for beams' }, // h*,upd
                    //{ attr:'duration',                                    abbr:'Chg',  name:'Charge Time',          unit:'s',           min:0,          time:1, default:0, scale:1,             desc:'Time to reach maximum charge (in seconds)' }, // h*
                    //{ attr:'dmgmul',                                      abbr:'DMul', name:'Damage Multiplier',    unit:'x',           min:0,                  default:0, scale:1,             desc:'Damage multiplier at full charge' }, // h*
                    //{ attr:'distdraw',   fdattr:'DistributorDraw',        abbr:'Dst',  name:'Distributor Draw',     unit:'MW',   bad:1, min:0,                  default:0, scale:2,             desc:'Weapons capacitor draw (in megawatts) per shot, or per second for beams' }, // h*,ucl,uhsl
                    //{ attr:'eps',                                         abbr:'EPS',  name:'Energy per Second',    unit:'MW/s', bad:1, min:0,                  default:0, scale:2,             desc:'Weapons capacitor draw (in megawatts per second), not including reload time' },
                    //{ attr:'seps',                                        abbr:'SEPS', name:'Sustained EPS',        unit:'MW/s', bad:1, min:0,                  default:0, scale:2,             desc:'Sustained weapons capacitor draw (in megawatts per second), including reload time' },
                    //{ attr:'thmload',    fdattr:'ThermalLoad',            abbr:'ThL',  name:'Thermal Load',                      bad:1, min:0,                  default:0, scale:1,             desc:'Waste heat generated per shot, or per second for beams' }, // h*,ucl,upd
                    //{ attr:'hps',                                         abbr:'HPS',  name:'Heat per Second',      unit:'/s',   bad:1, min:0,                  default:0, scale:1,             desc:'Waste heat generated per second, not including reload time' },
                    //{ attr:'shps',                                        abbr:'SHPS', name:'Sustained HPS',        unit:'/s',   bad:1, min:0,                  default:0, scale:1,             desc:'Waste heat generated per second, including reload time' },
                    //{ attr:'pierce',     fdattr:'ArmourPenetration',      abbr:'Prc',  name:'Armour Piercing',                          min:0,                  default:0, scale:0,             desc:'Armour pierce rating (compare to target ship armour hardness)' }, // h*
                    //{ attr:'maximumrng', fdattr:'MaximumRange',           abbr:'Rng',  name:'Maximum Range',        unit:'M',           min:0,                  default:0, scale:0,             desc:'Maximum range (in meters)' }, // h*,upd
                    //{ attr:'dmgfall',    fdattr:'FalloffRange',           abbr:'FOff', name:'Damage Falloff Start', unit:'M',           min:0,                  default:0, scale:0,             desc:'Range at which applied damage will begin to decrease (in meters)' }, // h*
                    //{ attr:'shotspd',    fdattr:'ShotSpeed',              abbr:'Spd',  name:'Shot Speed',           unit:'M/s',         min:0,                  default:0, scale:0,             desc:'Projectile speed (in meters per second)' }, // h*,upd
                    //{ attr:'rof',        fdattr:'RateOfFire',             abbr:'ROF',  name:'Rate of Fire',         unit:'/s',          min:0,                  default:1, scale:1,             desc:'Raw rate of fire (in shots per second), not including reload time' }, // h*,ucl,uhsl,upd
                    //{ attr:'srof',                                        abbr:'SROF', name:'Sustained ROF',        unit:'/s',          min:0,                  default:1, scale:1,             desc:'Sustained rate of fire (in shots per second), including reload time' },
                    //{ attr:'bstint',                                      abbr:'BInt', name:'Burst Interval',       unit:'s',    bad:1, min:0,                  default:1, scale:2,             desc:'Time between shots or busts (in seconds)' },
                    //{ attr:'bstrof',     fdattr:'BurstRateOfFire',        abbr:'BROF', name:'Burst Rate of Fire',   unit:'/s',          min:0,                  default:1, scale:1, modset:  1, desc:'Burst rate of fire (in shots per second)' }, // h*,upd
                    //{ attr:'bstsize',    fdattr:'BurstSize',              abbr:'BSz',  name:'Burst Size',                               min:1,          step:1, default:1, scale:0, modset:  1, desc:'Number of shots in a burst' }, // h*,upd
                    //{ attr:'ammoclip',   fdattr:'AmmoClipSize',           abbr:'Clip', name:'Ammo Clip Size',                           min:1,          step:1, default:0, scale:0,             desc:'Maximum ammo per clip before reloading' }, // h*,ucl,uhsl,upd,iscb,iss
                    //{ attr:'ammomax',    fdattr:'AmmoMaximum',            abbr:'Ammo', name:'Ammo Maximum',                             min:0,          step:1, default:0, scale:0,             desc:'Maximum reserve ammo to reload from' }, // h*,ucl,uhsl,upd,iscb
                    //{ attr:'rounds',     fdattr:'RoundsPerShot',          abbr:'Rnd',  name:'Rounds per Shot',                          min:1,          step:1, default:1, scale:0, modadd:  1, desc:'Number of rounds fired per shot' }, // h*
                    //{ attr:'rldtime',    fdattr:'ReloadTime',             abbr:'Rld',  name:'Reload Time',          unit:'s',    bad:1, min:0,          time:1, default:0, scale:0,             desc:'Time to reload (in seconds)' }, // h*,ucl,uhsl,upd
                    //{ attr:'brcdmg',     fdattr:'BreachDamage',           abbr:'Brc',  name:'Breach Damage',                            min:0,                  default:0, scale:1,             desc:'Damage to target modules when hull is breached' }, // h*
                    //{ attr:'minbrc',     fdattr:'MinBreachChance',        abbr:'NBrc', name:'Min Breach Chance',    unit:'%',           min:0, max:100,         default:0, scale:1,             desc:'Chance to breach a hull at full integrity' }, // h*
                    //{ attr:'maxbrc',     fdattr:'MaxBreachChance',        abbr:'XBrc', name:'Max Breach Chance',    unit:'%',           min:0, max:100,         default:0, scale:1,             desc:'Chance to breach a hull at zero integrity' }, // h*
                    //{ attr:'jitter',     fdattr:'Jitter',                 abbr:'Jtr',  name:'Jitter',               unit:'&deg;',bad:1, min:0, max:360,         default:0, scale:2, modadd:  1, desc:'Maximum accuracy deviation (in degrees)'}, // h*,upd
                    //{ attr:null,         fdattr:'WeaponMode' },
                    //{ attr:null,         fdattr:'DamageType',             abbr:'DTyp', name:'Damage Type',    values:['K','T','E','KT','KE','TK','TE','EK','ET','X'], default:'',               desc:'Damage type (kinetic/thermal/explosive)' },
                    //{ attr:'kinwgt',                                      abbr:'KinD', name:'Kinetic Damage',       unit:'%',           min:0, max:100,         default:0, scale:2, modset:  1, desc:'Kinetic portion of total damage' }, // h*
                    //{ attr:'thmwgt',                                      abbr:'ThmD', name:'Thermal Damage',       unit:'%',           min:0, max:100,         default:0, scale:2, modset:  1, desc:'Thermal portion of total damage' }, // h*
                    //{ attr:'expwgt',                                      abbr:'ExpD', name:'Explosive Damage',     unit:'%',           min:0, max:100,         default:0, scale:2, modset:  1, desc:'Explosive portion of total damage' }, // h*
                    //{ attr:'abswgt',                                      abbr:'AbsD', name:'Absolute Damage',      unit:'%',           min:0, max:100,         default:0, scale:2, modset:  1, desc:'Absolute portion of total damage' }, // h*
                    //{ attr:'cauwgt',                                      abbr:'CauD', name:'Caustic Damage',       unit:'%',           min:0, max:100,         default:0, scale:2, modset:  1, desc:'Caustic portion of total damage' }, // h*
                    //{ attr:'axewgt',                                      abbr:'AXeD', name:'Anti-Xeno Damage',     unit:'%',           min:0, max:100,         default:0, scale:2, modset:  1, desc:'Anti-Xeno portion of total damage' }, // h*
                    //{ attr:'genminmass', fdattr:'ShieldGenMinimumMass',   abbr:'NMas', name:'Minimum Mass',         unit:'T',           min:0,                  default:0, scale:1,             desc:'Minimum ship hull mass (in tons)' }, // isg
                    //{ attr:'genoptmass', fdattr:'ShieldGenOptimalMass',   abbr:'OMas', name:'Optimal Mass',         unit:'T',           min:0,                  default:0, scale:1,             desc:'Optimal ship hull mass (in tons)' }, // isg
                    //{ attr:'genmaxmass', fdattr:'ShieldGenMaximumMass',   abbr:'XMas', name:'Maximum Mass',         unit:'T',           min:0,                  default:0, scale:1,             desc:'Maximum ship hull mass (in tons)' }, // isg
                    //{ attr:'genminmul',  fdattr:'ShieldGenMinStrength',   abbr:'NStr', name:'Minimum Strength',     unit:'%',           min:0,                  default:0, scale:0,             desc:'Minimum strength modifier' }, // isg
                    //{ attr:'genoptmul',  fdattr:'ShieldGenStrength',      abbr:'OStr', name:'Optimal Strength',     unit:'%',           min:0,                  default:0, scale:0,             desc:'Optimal strength modifier' }, // isg
                    //{ attr:'genmaxmul',  fdattr:'ShieldGenMaxStrength',   abbr:'XStr', name:'Maximum Strength',     unit:'%',           min:0,                  default:0, scale:0,             desc:'Maximum strength modifier' }, // isg
                    //{ attr:'genrate',    fdattr:'RegenRate',              abbr:'Rgn',  name:'Regen Rate',           unit:'/s',          min:0,                  default:0, scale:1,             desc:'Shield recharge rate while up (in units per second)' }, // isg
                    //{ attr:'bgenrate',   fdattr:'BrokenRegenRate',        abbr:'BkR',  name:'Broken Regen Rate',    unit:'/s',          min:0,                  default:0, scale:1,             desc:'Shield recharge rate while down (in units per second)' }, // isg
                    //{ attr:'genpwr',     fdattr:'EnergyPerRegen',         abbr:'Dst',  name:'Distributor Draw',     unit:'MW',   bad:1, min:0,                  default:0, scale:2,             desc:'Systems capacitor draw (in megawatts) to recharge 1 shield unit' }, // isg
                    //{ attr:'fsdoptmass', fdattr:'FSDOptimalMass',         abbr:'OMas', name:'Optimised Mass',       unit:'T',           min:0,                  default:0, scale:1,             desc:'Optimal ship mass (in tons)' }, // cfsd,cfsdo
                    //{ attr:'fsdheat',    fdattr:'FSDHeatRate',            abbr:'ThL',  name:'Thermal Load',         unit:'/s',   bad:1, min:0,                  default:0, scale:1,             desc:'Waste heat generated (in units per second)' }, // cfsd,cfsdo
                    //{ attr:'maxfuel',    fdattr:'MaxFuelPerJump',         abbr:'Max',  name:'Max Fuel per Jump',    unit:'T',           min:0,                  default:0, scale:2,             desc:'Maximum fuel use per jump (in tons)' }, // cfsd,cfsdo
    engminmass,     //{ attr:'engminmass', fdattr:'EngineMinimumMass',      abbr:'NMas', name:'Minimum Mass',         unit:'T',           min:0,                  default:0, scale:1,             desc:'Minimum ship mass (in tons)' }, // ct
    engoptmass,     //{ attr:'engoptmass', fdattr:'EngineOptimalMass',      abbr:'OMas', name:'Optimal Mass',         unit:'T',           min:0,                  default:0, scale:1,             desc:'Optimal ship mass (in tons)' }, // ct
    engmaxmass,     //{ attr:'engmaxmass', fdattr:'MaximumMass',            abbr:'XMas', name:'Maximum Mass',         unit:'T',           min:0,                  default:0, scale:1,             desc:'Maximum ship mass (in tons)' }, // ct
    engminmul,      //{ attr:'engminmul',  fdattr:'EngineMinPerformance',   abbr:'NMul', name:'Minimum Multiplier',   unit:'%',           min:0,                  default:0, scale:0,             desc:'Minimum performance modifier' }, // ct
    engoptmul,      //{ attr:'engoptmul',  fdattr:'EngineOptPerformance',   abbr:'OMul', name:'Optimal Multiplier',   unit:'%',           min:0,                  default:0, scale:0,             desc:'Optimal performance modifier' }, // ct
    engmaxmul,      //{ attr:'engmaxmul',  fdattr:'EngineMaxPerformance',   abbr:'XMul', name:'Maximum Multiplier',   unit:'%',           min:0,                  default:0, scale:0,             desc:'Maximum performance modifier' }, // ct
    minmulspd,      //{ attr:'minmulspd',                                   abbr:'NSMul',name:'Min Speed Mult',       unit:'%',           min:0,                  default:'engminmul', scale:0,   desc:'Minimum speed modifier' }, // ct
    optmulspd,      //{ attr:'optmulspd',                                   abbr:'OSMul',name:'Opt Speed Mult',       unit:'%',           min:0,                  default:'engoptmul', scale:0,   desc:'Optimal speed modifier' }, // ct
    maxmulspd,      //{ attr:'maxmulspd',                                   abbr:'XSMul',name:'Max Speed Mult',       unit:'%',           min:0,                  default:'engmaxmul', scale:0,   desc:'Maximum speed modifier' }, // ct
    minmulacc,      //{ attr:'minmulacc',                                   abbr:'NAMul',name:'Min Acceleration Mult',unit:'%',           min:0,                  default:'engminmul', scale:0,   desc:'Minimum acceleration modifier' }, // ct
    optmulacc,      //{ attr:'optmulacc',                                   abbr:'OAMul',name:'Opt Acceleration Mult',unit:'%',           min:0,                  default:'engoptmul', scale:0,   desc:'Optimal acceleration modifier' }, // ct
    maxmulacc,      //{ attr:'maxmulacc',                                   abbr:'XAMul',name:'Max Acceleration Mult',unit:'%',           min:0,                  default:'engmaxmul', scale:0,   desc:'Maximum acceleration modifier' }, // ct
    minmulrot,      //{ attr:'minmulrot',                                   abbr:'NRMul',name:'Min Rotation Mult',    unit:'%',           min:0,                  default:'engminmul', scale:0,   desc:'Minimum rotation modifier' }, // ct
    optmulrot,      //{ attr:'optmulrot',                                   abbr:'ORMul',name:'Opt Rotation Mult',    unit:'%',           min:0,                  default:'engoptmul', scale:0,   desc:'Optimal rotation modifier' }, // ct
    maxmulrot,      //{ attr:'maxmulrot',                                   abbr:'XRMul',name:'Max Rotation Mult',    unit:'%',           min:0,                  default:'engmaxmul', scale:0,   desc:'Maximum rotation modifier' }, // ct
                    //{ attr:'engheat',    fdattr:'EngineHeatRate',         abbr:'ThL',  name:'Thermal Load',         unit:'/s',   bad:1, min:0,                  default:0, scale:1,             desc:'Waste heat generated (in units per second) at top speed' }, // ct
                    //{ attr:'pwrcap',     fdattr:'PowerCapacity',          abbr:'PwC',  name:'Power Capacity',       unit:'MW',          min:0,                  default:0, scale:2,             desc:'Power output (in megawatts)' }, // cpp
                    //{ attr:'pwrbst',                                      abbr:'PwB',  name:'Power Boost',          unit:'%',           min:-100,               default:0, scale:1,             desc:'Power output bonus' }, // cpd (Guardian)
                    //{ attr:'heateff',    fdattr:'HeatEfficiency',         abbr:'HEf',  name:'Heat Efficiency',      unit:'/MW',  bad:1, min:0,                  default:0, scale:2,             desc:'Waste heat generated (in units per megawatt consumed)' }, // cpp
                    //{ attr:'wepcap',     fdattr:'WeaponsCapacity',        abbr:'WpC',  name:'Weapons Capacity',     unit:'MW',          min:0,                  default:0, scale:2,             desc:'Weapons capacitor capacity (nonsensically, in megawatts)' }, // cpd
                    //{ attr:'wepchg',     fdattr:'WeaponsRecharge',        abbr:'WpR',  name:'Weapons Recharge',     unit:'MJ/s',        min:0,                  default:0, scale:2,             desc:'Weapons capacitor recharge rate (in megajoules per second)' }, // cpd
                    //{ attr:'engcap',     fdattr:'EnginesCapacity',        abbr:'EnC',  name:'Engines Capacity',     unit:'MW',          min:0,                  default:0, scale:2,             desc:'Engines capacitor capacity (nonsensically, in megawatts)' }, // cpd
                    //{ attr:'engchg',     fdattr:'EnginesRecharge',        abbr:'EnR',  name:'Engines Recharge',     unit:'MJ/s',        min:0,                  default:0, scale:2,             desc:'Engines capacitor recharge rate (in megajoules per second)' }, // cpd
                    //{ attr:'syscap',     fdattr:'SystemsCapacity',        abbr:'SyC',  name:'Systems Capacity',     unit:'MW',          min:0,                  default:0, scale:2,             desc:'Systems capacitor capacity (nonsensically, in megawatts)' }, // cpd
                    //{ attr:'syschg',     fdattr:'SystemsRecharge',        abbr:'SyR',  name:'Systems Recharge',     unit:'MJ/s',        min:0,                  default:0, scale:2,             desc:'Systems capacitor recharge rate (in megajoules per second)' }, // cpd
                    //{ attr:'hullbst',    fdattr:'DefenceModifierHealthMultiplier',abbr:'HuB',name:'Hull Boost',     unit:'%',           min:-100,               default:0, scale:1, modmod:100, desc:'Hull strength bonus' }, // cbh
                    //{ attr:'hullrnf',    fdattr:'DefenceModifierHealthAddition',  abbr:'HuR',name:'Hull Reinforcement',                 min:0,                  default:0, scale:0,             desc:'Additional hull strength' }, // ihrp,imahrp
                    //{ attr:'shieldbst',  fdattr:'DefenceModifierShieldMultiplier',abbr:'ShB',name:'Shield Boost',   unit:'%',           min:-100,               default:0, scale:1, modmod:100, desc:'Shield strength bonus' }, // usb
                    //{ attr:'shieldrnf',  fdattr:'DefenceModifierShieldAddition',  abbr:'ShR',name:'Shield Reinforcement',               min:0,                  default:0, scale:0,             desc:'Additional shield strength' }, // isrp
                    //{ attr:'absres',     fdattr:'CollisionResistance',    abbr:'AbR',  name:'Absolute Resistance',  unit:'%',           min:-1000,max:100,      default:0, scale:1, modmod:-100,desc:'Resistance to absolute/collision damage' },
                    //{ attr:'kinres',     fdattr:'KineticResistance',      abbr:'KiR',  name:'Kinetic Resistance',   unit:'%',           min:-1000,max:100,      default:0, scale:1, modmod:-100,desc:'Resistance to kinetic damage' }, // usb,cbh,ihrp,isg
                    //{ attr:'thmres',     fdattr:'ThermicResistance',      abbr:'ThR',  name:'Thermal Resistance',   unit:'%',           min:-1000,max:100,      default:0, scale:1, modmod:-100,desc:'Resistance to thermal damage' }, // usb,cbh,ihrp,isg
                    //{ attr:'expres',     fdattr:'ExplosiveResistance',    abbr:'ExR',  name:'Explosive Resistance', unit:'%',           min:-1000,max:100,      default:0, scale:1, modmod:-100,desc:'Resistance to explosive damage' }, // usb,cbh,ihrp,isg
                    //{ attr:'caures',     fdattr:'CausticResistance',      abbr:'CaR',  name:'Caustic Resistance',   unit:'%',           min:-1000,max:100,      default:0, scale:1, modmod:-100,desc:'Resistance to caustic damage' }, // ihrp,imahrp
                    //{ attr:'axeres',                                      abbr:'AXR',  name:'Anti-Xeno Resistance', unit:'%',           min:-1000,max:100,      default:0, scale:1, modmod:-100,desc:'Resistance to anti-xeno damage' },
                    //{ attr:'timerng',    fdattr:'FSDInterdictorRange',    abbr:'Rng',  name:'Range',                unit:'s',           min:0,          time:1, default:0, scale:0,             desc:'Maximum target range (in seconds to intercept)' }, // ifsdi
                    //{ attr:'facinglim',  fdattr:'FSDInterdictorFacingLimit',abbr:'Ang',name:'Facing Limit',         unit:'&deg;',       min:0, max:360,         default:0, scale:2,             desc:'Maximum target angle (in degrees)' }, // ifsdi
                    //{ attr:'scanrng',    fdattr:'ScannerRange',           abbr:'Rng',  name:'Scanner Range',        unit:'M',           min:0,                  default:0, scale:0,             desc:'Maximum scan range (in meters)' }, // ucs,uex,ufsws,ukws,upwa
                    //{ attr:null,         fdattr:'DiscoveryScannerRange',  abbr:'ARng', name:'Active Range',         unit:'LS',          min:0,                  default:0, scale:0,             desc:'Maximum active scan range (in light-seconds)' },
                    //{ attr:null,         fdattr:'DiscoveryScannerPassiveRange',abbr:'PRng',name:'Passive Range',    unit:'LS',          min:0,                  default:0, scale:2,             desc:'Automatic passive scan range (in light-seconds)' },
                    //{ attr:'maxangle',   fdattr:'MaxAngle',               abbr:'Ang',  name:'Max Angle',            unit:'&deg;',       min:0, max:360,         default:0, scale:2,             desc:'Maximum scan angle (in degrees)' }, // ucs,uex,ufsws,ukws,upwa
                    //{ attr:'scantime',   fdattr:'ScannerTimeToScan',      abbr:'Time', name:'Scan Time',            unit:'s',    bad:1, min:0,          time:1, default:0, scale:1,             desc:'Time to scan (in seconds)' }, // ucs,uex,ufsws,ukws,upwa
                    //{ attr:'jamdur',     fdattr:'ChaffJamDuration',       abbr:'Dur',  name:'Jam Duration',         unit:'s',           min:0,          time:1, default:0, scale:0,             desc:'Duration of jamming effect (in seconds)' }, // ucl
                    //{ attr:'ecmrng',     fdattr:'ECMRange',               abbr:'Rng',  name:'Range',                unit:'M',           min:0,                  default:0, scale:0,             desc:'Maximum effective range (in meters)' }, // uec
                    //{ attr:'ecmdur',     fdattr:'ECMTimeToCharge',        abbr:'Dur',  name:'Duration',             unit:'s',           min:0,          time:1, default:0, scale:0,             desc:'Maximum charge duration (in seconds)' }, // uec
                    //{ attr:'ecmpwr',     fdattr:'ECMActivePowerConsumption',abbr:'PDr',name:'Active Power Draw',    unit:'MW',   bad:1, min:0,                  default:0, scale:2,             desc:'Systems capacitor draw (in megawatts per use)' }, // uec
                    //{ attr:'ecmheat',    fdattr:'ECMHeat',                abbr:'ThL',  name:'Thermal Load',         unit:'/s',   bad:1, min:0,                  default:0, scale:1,             desc:'Waste heat generated (in units per second)' }, // uec
                    //{ attr:'ecmcool',    fdattr:'ECMCooldown',            abbr:'Cool', name:'Cool Down',            unit:'s',    bad:1, min:0,          time:1, default:0, scale:0,             desc:'Minimum time between uses (in seconds)' }, // uec
                    //{ attr:'hsdur',      fdattr:'HeatSinkDuration',       abbr:'Dur',  name:'Duration',             unit:'s',           min:0,          time:1, default:0, scale:0,             desc:'Duration of heat dumping effect (in seconds)' }, // uhsl
                    //{ attr:'thmdrain',   fdattr:'ThermalDrain',           abbr:'ThD',  name:'Thermal Drain',        unit:'/s',          min:0,                  default:0, scale:1,             desc:'Waste heat drained (in units per second)' }, // uhsl
                    //{ attr:'vslots',     fdattr:'NumBuggySlots',          abbr:'Slots',name:'Vehicle Slots',                            min:0,          step:1, default:0, scale:0, modadd:  1, desc:'Number of vehicle slots' }, // ifh,ipvh
                    //{ attr:'vcount',                                      abbr:'Vcls', name:'Vehicle Count',                            min:1,          step:1, default:1, scale:0,             desc:'Maximum number of vehicles that can be deployed per slot' }, // ifh
                    //{ attr:'cargocap',   fdattr:'CargoCapacity',          abbr:'Cap',  name:'Cargo Capacity',                           min:0,          step:1, default:0, scale:0,             desc:'Maximum cargo capacity' }, // icr
                    //{ attr:'maxlimpet',  fdattr:'MaxActiveDrones',        abbr:'Max',  name:'Max Active Limpets',                       min:0,          step:1, default:0, scale:0, modadd:  1, desc:'Maximum active limpets' }, // i*lc
                    //{ attr:'lpactrng',                                    abbr:'ARng', name:'Active Range',         unit:'M',           min:0,                  default:0, scale:0,             desc:'Maximum limpet range (in meters)' }, // i*lc
                    //{ attr:'targetrng',  fdattr:'DroneTargetRange',       abbr:'Rng',  name:'Target Range',         unit:'M',           min:0,                  default:'lpactrng', scale:0,    desc:'Maximum limpet target range (in meters)' }, // ihblc
                    //{ attr:'limpettime', fdattr:'DroneLifeTime',          abbr:'Time', name:'Limpet Life Time',     unit:'s',           min:0,          time:1, default:0, scale:0,             desc:'Maximum limpet life time (in seconds)' }, // i*lc
                    //{ attr:'maxspd',     fdattr:'DroneSpeed',             abbr:'Spd',  name:'Maximum Speed',        unit:'M/s',         min:0,                  default:0, scale:0,             desc:'Maximum limpet speed (in meters per second)' }, // i*lc
                    //{ attr:'multispd',   fdattr:'DroneMultiTargetSpeed',  abbr:'MSpd', name:'Multi-Target Speed',   unit:'M/s',         min:0,                  default:0, scale:0,             desc:'Multi-target limpet speed (in meters per second)' }, // iclc
                    //{ attr:'fuelxfer',   fdattr:'DroneFuelCapacity',      abbr:'Xfer', name:'Fuel Transfer',        unit:'T',           min:0,                  default:0, scale:1,             desc:'Maximum limpt fuel transfer amount (in tons)' }, // iftlc
                    //{ attr:'lmprepcap',  fdattr:'DroneRepairCapacity',    abbr:'Cap',  name:'Repair Capacity',                          min:0,          step:1, default:0, scale:0,             desc:'Maximum repair material capacity' }, // idlc,irlc
                    //{ attr:'hacktime',   fdattr:'DroneHackingTime',       abbr:'Hack', name:'Hacking Time',         unit:'s',    bad:1, min:0,          time:1, default:0, scale:0,             desc:'Time to hack (in seconds)' }, // ihblc,inlc
                    //{ attr:'mincargo',   fdattr:'DroneMinJettisonedCargo',abbr:'NCgo', name:'Minimum Cargo',                            min:0,          step:1, default:0, scale:0, modadd:  1, desc:'Minimum cargo yield' }, // ihblc
                    //{ attr:'maxcargo',   fdattr:'DroneMaxJettisonedCargo',abbr:'XCgo', name:'Maximum Cargo',                            min:0,          step:1, default:0, scale:0, modadd:  1, desc:'Maximum cargo yield' }, // ihblc
                    ////	{ attr:'minebonus',                                   abbr:'MnBn', name:'Mining Bonus',                             min:0,                  default:0, scale:1,             desc:'' }, // iplc
                    //{ attr:'scooprate',  fdattr:'FuelScoopRate',          abbr:'Rate', name:'Scoop Rate',           unit:'T/s',         min:0,                  default:0, scale:3,             desc:'Fuel scroop rate (in tons per second)' }, // ifs
                    //{ attr:'fuelcap',    fdattr:'FuelCapacity',           abbr:'Cap',  name:'Fuel Capacity',        unit:'T',           min:0,                  default:0, scale:1,             desc:'Maximum fuel capacity (in tons)' }, // cft
                    //{ attr:'emgcylife',  fdattr:'OxygenTimeCapacity',     abbr:'EmLf', name:'Emergency Life',       unit:'s',           min:0,          time:1, default:0, scale:0,             desc:'Maximum emergency oxygen time (in seconds)' }, // cls
                    //{ attr:'bins',       fdattr:'RefineryBins',           abbr:'Bins', name:'Bin Count',                                min:0,          step:1, default:0, scale:0, modadd:  1, desc:'Number of bins' }, // ir
                    //{ attr:'afmrepcap',  fdattr:'AFMRepairCapacity',      abbr:'Cap',  name:'Repair Capacity',                          min:0,          step:1, default:0, scale:0,             desc:'Maximum repair material capacity' }, // iafmu
                    //{ attr:'repaircon',  fdattr:'AFMRepairConsumption',   abbr:'Cns',  name:'Consumption',          unit:'/s',          min:0,                  default:0, scale:1,             desc:'Rate of repair material consumption (in units per second)' }, // iafmu
                    //{ attr:'repairrtg',  fdattr:'AFMRepairPerAmmo',       abbr:'Rtg',  name:'Repair Rating',                            min:0,                  default:1, scale:3,             desc:'Module integrity repaired per material consumed' }, // iafmu
                    //{ attr:'maxrng',     fdattr:'MaxRange',               abbr:'Rng',  name:'Max Range',            unit:'KM',          min:0,                  default:0, scale:2,             desc:'Maximum range (in kilometers)' }, // cs
                    //{ attr:'scanangle',  fdattr:'SensorTargetScanAngle',  abbr:'Ang',  name:'Scan Angle',           unit:'&deg;',       min:0, max:360,         default:0, scale:2,             desc:'Maximum scan angle (in degrees)' }, // cs
                    //{ attr:'typemis',    fdattr:'Range',                  abbr:'Typ',  name:'Typical Emission',     unit:'M',           min:0,                  default:0, scale:0,             desc:'Range to resolve a contact with typical emissions (in meters)' }, // cs // in KM in-game, but in M in API BaseValue
                    //{ attr:null,         fdattr:'VehicleCargoCapacity' },
                    //{ attr:null,         fdattr:'VehicleHullMass' },
                    //{ attr:null,         fdattr:'VehicleFuelCapacity' },
                    //{ attr:null,         fdattr:'VehicleArmourHealth' },
                    //{ attr:null,         fdattr:'VehicleShieldHealth' },
                    //{ attr:null,         fdattr:'FighterMaxSpeed' },
                    //{ attr:null,         fdattr:'FighterBoostSpeed' },
                    //{ attr:null,         fdattr:'FighterPitchRate' },
                    //{ attr:null,         fdattr:'FighterDPS' },
                    //{ attr:null,         fdattr:'FighterYawRate' },
                    //{ attr:null,         fdattr:'FighterRollRate' },
                    //{ attr:'cabincap',   fdattr:'CabinCapacity',          abbr:'Cap',  name:'Cabin Capacity',                           min:0,          step:1, default:0, scale:0,             desc:'Maximum passenger capacity' }, // ipc
                    //{ attr:'cabincls',   fdattr:'CabinClass',             abbr:'Cls',  name:'Cabin Class',    values:['','E','B','F','L'],                      default:'',                     desc:'Passenger cabin quality class (economy/business/first/luxury)' }, // ipc
                    //{ attr:'barrierrng', fdattr:'DisruptionBarrierRange', abbr:'Rng',  name:'Range',                unit:'M',           min:0,                  default:0, scale:0,             desc:'Maximum range (in meters) at full charge' }, // uex
                    //{ attr:'barrierdur', fdattr:'DisruptionBarrierChargeDuration',abbr:'Chg',name:'Charge Time',    unit:'s',           min:0,          time:1, default:0, scale:0,             desc:'Time to charge (in seconds)' }, // uex
                    //{ attr:'barrierpwr', fdattr:'DisruptionBarrierActivePower',abbr:'PDr',name:'Active Power Draw', unit:'MW/s', bad:1, min:0,                  default:0, scale:2,             desc:'Systems capacitor draw (in megawatts per second)' }, // uex
                    //{ attr:'barriercool',fdattr:'DisruptionBarrierCooldown',abbr:'Cool',name:'Cool Down',           unit:'s',    bad:1, min:0,          time:1, default:0, scale:0,             desc:'Minimum time between uses (in seconds)' }, // uex
                    //{ attr:null,         fdattr:'WingDamageReduction' },
                    //{ attr:null,         fdattr:'WingMinDuration' },
                    //{ attr:null,         fdattr:'WingMaxDuration' },
                    //{ attr:null,         fdattr:'ShieldSacrificeAmountRemoved' },
                    //{ attr:null,         fdattr:'ShieldSacrificeAmountGiven' },
                    //{ attr:'jumpbst',    fdattr:'FSDJumpRangeBoost',      abbr:'JBst', name:'Jump Range Bonus',     unit:'LY',          min:0,                  default:0, scale:2,             desc:'Jump range bonus (in light-years)' }, // ifsdb
                    //{ attr:null,         fdattr:'FSDFuelUseIncrease' },
                    //{ attr:null,         fdattr:'BoostSpeedMultiplier' },
                    //{ attr:null,         fdattr:'BoostAugmenterPowerUse' },
                    //{ attr:'dmgprot',    fdattr:'ModuleDefenceAbsorption',abbr:'DmgP', name:'Damage Protection',    unit:'%',           min:0, max:100,         default:0, scale:0,             desc:'Portion of incoming module damage that is absorbed' }, // imrp
                    //{ attr:'scanrngmod', fdattr:'DSS_RangeMult',          abbr:'RngM', name:'Scan Range Multiplier',unit:'%',                                   default:0, scale:1, modmod:100, desc:'Modifies maximum range to scan stellar bodies' }, // TODO: delete?
                    //{ attr:'scanangmod', fdattr:'DSS_AngleMult',          abbr:'AngM', name:'Scan Angle Multiplier',unit:'%',                                   default:0, scale:1, modmod:100, desc:'Modifies maximum angle to scan stellar bodies' }, // TODO: delete?
                    //{ attr:'scanratemod',fdattr:'DSS_RateMult',           abbr:'RteM', name:'Scan Rate Multiplier', unit:'%',                                   default:0, scale:1, modmod:100, desc:'Modifies time to scan stellar bodies' }, // TODO: delete?
                    //{ attr:'proberad',   fdattr:'DSS_PatchRadius',        abbr:'PRad', name:'Probe Radius',         unit:'% ', /* space is kludgy but easy */   default:0, scale:1,             desc:'Modifies surface scan probe range' }, // iss
                    //{ attr:'mlctype',                                     abbr:'Type', name:'Controller Type',      values:['','M','O','R','X','U'],            default:'',                     desc:'Multi Limpet Controller Type (mining/operations/rescue/xeno/universal)' }, // imlc
                    //{ attr:'agzresist',  fdattr:'GuardianModuleResistance',abbr:'AGZR',name:'Anti Guardian Zone Resistance', values:['','Active'],              default:'',                     desc:'Resistance to Thargoid anti-Guardian field' }, // hextp
                    //{ attr:'sco',                                         abbr:'SCO',    name:'Supercruise Overcharge', values:['','Available'],                default:'',                     desc:'Capable of activating Supercruise Overcharge mode' }, // cfsdo
                    //{ attr:'scospd',                                      abbr:'SCOSpd', name:'SCO Max Speed Increase', unit:'%',       min:0,                  default:0, scale:0,             desc:'Supercruise speed bonus during Overcharge mode' }, // cfsdo
                    //{ attr:'scoacc',                                      abbr:'SCOAcc', name:'SCO Max Acceleration Rate',              min:0,                  default:0, scale:3,             desc:'Supercruise acceleration rate bonus during Overcharge mode' }, // cfsdo
                    //{ attr:'scoheat',                                     abbr:'SCOHt',  name:'SCO Heat Generation Rate',               min:0,                  default:0, scale:3,             desc:'Additional thermal load during Overcharge mode' }, // cfsdo
                    //{ attr:'scoconint',                                   abbr:'SCOCnIn',name:'SCO Control Interference',               min:0,                  default:0, scale:3,             desc:'Control interference during Overcharge mode' }, // cfsdo // TODO: clarify description
                    //{ attr:'scofuel',                                     abbr:'SCOFu',  name:'SCO Fuel Consumption',                   min:0,                  default:0, scale:3,             desc:'Fuel consumption during Overcharge mode' }, // cfsdo // TODO: clarify description
};

class ShipAttr {
public:
    ShipAttr(Attr attr, const json5pp::value& jv);
    //attr:'faction',    hidden:1,                        abbr:'Fac',  name:'Faction',        values:['','Alliance','Empire','Federation'],     default:'',                     desc:'Faction membership required to purchase'
    const Attr attr;
    const json5pp::value& jvalue;
    std::string id;
    bool hidden;
    bool bad;
    std::string abbr;
    std::string name;
    std::string desc;
    bool modset;
    bool modadd;
    std::string unit;
    std::optional<double> modmod;
    //std::optional<double> scale;
    std::optional<double> min;
    std::optional<double> max;
    std::optional<double> step;
    double default_value;
    ShipAttr* default_attr;
};
class ShipSlot {
public:
    ShipSlot(ShipStats& ship, std::string name, ShipSlotGroup group);

    ShipStats& ship;
    const std::string name;
    const ShipSlotGroup group;

    void setModule(const std::string& name);
    double getBlueprintGradeRollAttrModifier(ShipAttr& attr);
    void setEngineering(const std::string& blueprint, int level, float quality, const std::string& effect);

    double getEffectiveAttrValue(Attr attr) {
        return getAttrValue(attr, true);
    }
    double getBaseAttrValue(Attr attr) {
        return getAttrValue(attr, false);
    }
    double getBaseAttrModifier(Attr attr);
    double getRelatedAttrModifier(Attr attr);
    double getExperimentalAttrModifier(Attr attr);
    double getAttrModifierSum(Attr attr, double modifier1, double modifier2);
    double getEffectiveAttrModifier(Attr attr);
    double getAttrValue(Attr attr, double value, double modifier);
    double getAttrValue(Attr attr, bool modified);
    double getModuleAttrValue(const json5pp::value& module, Attr attr, double modifier=DNaN);

    std::string moduleName;
    std::string blueprintName;
    std::string effectName;
    int blueprintLevel {};
    float blueprintQuality {};
    std::map<Attr,double> attrModifier;
    std::map<Attr,bool> attrOverride;

    const json5pp::value* module {};
    const json5pp::value* blueprint {};
    const json5pp::value* effect {};
};

class ShipStats {
public:
    ShipStats(const std::string &type, const json5pp::value &jship);
    void setSlotModule(const json5pp::value &jvalue);
    void updateStats();

    double getRotationScale(Axis::Type at, int speed_percent);
    double getRotationSpeed(Axis::Type at, int speed_percent);
    double getPitchSpeed(int speed_percent);
    double getYawSpeed(int speed_percent);
    double getRollSpeed(int speed_percent);
    double getThrustSpeed();
    double getForwardAccel();
    double getReverseAccel();

    const std::string type;
    const json5pp::value &jship;

private:
    std::array<double, enum_count<Attr>()> stats;
    float cruise_rot[3][3]; // Axis::type + at speed [0,50,100]
    float space_rot[3][3]; // Axis::type + at speed [0,50,100]

    void updateStat(ShipSlot& slot, Attr attr);
    eddb::ShipSlot& getSlot(const std::string& name);
    double getMassCurveMultiplier(double mass, double minMass, double optMass, double maxMass, double minMul, double optMul, double maxMul);
    double getMassRotMultiplier();
    double getMassSpdMultiplier();

    std::map<std::string, eddb::ShipSlot> slots;
};

typedef std::shared_ptr<ShipStats> spShipStats;

bool loadEDDB();
spShipStats initShipStats(const std::string &type);
void setShipStats(spShipStats shipStats);
spShipStats getShipStats();

} // namespace eddb

#endif //EDROBOT_SHIPSTATS_H
