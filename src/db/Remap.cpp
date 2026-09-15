//
// Created by mkizub on 08.09.2026.
//

#include "../pch.h"

#include "DB.h"
#include "JsEnum.h"
#include <glaze/beve.hpp>
#include <glaze/json.hpp>

namespace db {

JsAllegiance JsAllegiance::instance {
    {1, "Alliance"},
    {2, "Empire"},
    {3, "Federation"},
    {4, "Frontline Solutions"},
    {5, "Guardian"},
    {6, "Independent"},
    {7, "Pilots Federation"},
    {8, "Thargoid"},
};

JsGovernment JsGovernment::instance {
    {1, "None"},
    {2, "Anarchy"},
    {3, "Communism"},
    {4, "Confederacy"},
    {5, "Cooperative"},
    {6, "Corporate"},
    {7, "Democracy"},
    {8, "Dictatorship"},
    {9, "Engineer"},
    {10, "Feudal"},
    {11, "Megaconstruction"},
    {12, "Patronage"},
    {13, "Prison"},
    {14, "Prison Colony"},
    {15, "Private Ownership"},
    {16, "Theocracy"},
};

JsEconomy JsEconomy::instance {
    {1, "None"},
    {2, "Agriculture"},
    {3, "Colony"},
    {4, "Extraction"},
    {5, "High Tech"},
    {6, "Industrial"},
    {7, "Military"},
    {8, "Prison"},
    {9, "Private Enterprise"},
    {10, "Refinery"},
    {11, "Repair"},
    {12, "Rescue"},
    {13, "Service"},
    {14, "Terraforming"},
    {15, "Tourism"},
};

JsSecurity JsSecurity::instance {
    {1, "Anarchy"},
    {2, "Low"},
    {3, "Medium"},
    {4, "High"},
};

JsFactionState JsFactionState::instance {
    {1, "None"},
    {2, "Blight"},
    {3, "Boom"},
    {4, "Bust"},
    {5, "Civil Liberty"},
    {6, "Civil Unrest"},
    {7, "Civil War"},
    {8, "Drought"},
    {9, "Election"},
    {10, "Expansion"},
    {11, "Famine"},
    {12, "Infrastructure Failure"},
    {13, "Investment"},
    {14, "Lockdown"},
    {15, "Natural Disaster"},
    {16, "Outbreak"},
    {17, "Pirate Attack"},
    {18, "Public Holiday"},
    {19, "Retreat"},
    {20, "Terrorist Attack"},
    {21, "War"},
};

JsPower JsPower::instance {
    {1, "Aisling Duval"},
    {2, "A. Lavigny-Duval"},
    {3, "Archon Delaine"},
    {4, "Denton Patreus"},
    {5, "Edmund Mahon"},
    {6, "Felicia Winters"},
    {7, "Jerome Archer"},
    {8, "Li Yong-Rui"},
    {9, "Nakato Kaine"},
    {10, "Pranav Antal"},
    {11, "Yuri Grom"},
    {12, "Zemina Torval"},
};

JsPowerState JsPowerState::instance {
    {1, "Unoccupied"},
    {2, "Exploited"},
    {3, "Fortified"},
    {4, "Stronghold"},
};

JsThargoidState JsThargoidState::instance {
    {1, "None"},
    {2, "Thargoid Controlled"},
    {3, "Thargoid Harvest"},
    {4, "Thargoid Probing"},
    {5, "Thargoid Recovery"},
    {6, "Thargoid Stronghold"},
};

JsParentBodyType JsParentBodyType::instance {
    {1, "Star"},
    {2, "Planet"},
    {3, "Null"}, // Barycentre
    {4, "Ring"},
};

JsBodyType JsBodyType::instance {
    {1, "Star"},
    {2, "Planet"},
    {3, "Barycentre"},
    {4, "Ring"},
    {5, "Asteroid Cluster"},
};

JsBodySubType JsBodySubType::instance {
    // planets
    {1, "Ammonia world"},
    {2, "Class I gas giant"},
    {3, "Class II gas giant"},
    {4, "Class III gas giant"},
    {5, "Class IV gas giant"},
    {6, "Class V gas giant"},
    {7, "Earth-like world"},
    {8, "Gas giant with ammonia-based life"},
    {9, "Gas giant with water-based life"},
    {10, "Helium gas giant"},
    {11, "Helium-rich gas giant"},
    {12, "High metal content world"},
    {13, "Icy body"},
    {14, "Metal-rich body"},
    {15, "Rocky Ice world"},
    {16, "Rocky body"},
    {17, "Water giant"},
    {18, "Water world"},
    // stars
    {21, "A (Blue-White super giant) Star"},
    {22, "A (Blue-White) Star"},
    {23, "B (Blue-White super giant) Star"},
    {24, "B (Blue-White) Star"},
    {25, "Black Hole"},
    {26, "C Star"},
    {27, "CJ Star"},
    {28, "CN Star"},
    {29, "F (White super giant) Star"},
    {30, "F (White) Star"},
    {31, "G (White-Yellow super giant) Star"},
    {32, "G (White-Yellow) Star"},
    {33, "Herbig Ae/Be Star"},
    {34, "K (Yellow-Orange giant) Star"},
    {35, "K (Yellow-Orange) Star"},
    {36, "L (Brown dwarf) Star"},
    {37, "M (Red dwarf) Star"},
    {38, "M (Red giant) Star"},
    {39, "M (Red super giant) Star"},
    {40, "MS-type Star"},
    {41, "Neutron Star"},
    {42, "O (Blue-White) Star"},
    {43, "S-type Star"},
    {44, "Supermassive Black Hole"},
    {45, "T (Brown dwarf) Star"},
    {46, "T Tauri Star"},
    {47, "White Dwarf (D) Star"},
    {48, "White Dwarf (DA) Star"},
    {49, "White Dwarf (DAB) Star"},
    {50, "White Dwarf (DAV) Star"},
    {51, "White Dwarf (DAZ) Star"},
    {52, "White Dwarf (DB) Star"},
    {53, "White Dwarf (DBV) Star"},
    {54, "White Dwarf (DBZ) Star"},
    {55, "White Dwarf (DC) Star"},
    {56, "White Dwarf (DCV) Star"},
    {57, "White Dwarf (DQ) Star"},
    {58, "Wolf-Rayet C Star"},
    {59, "Wolf-Rayet N Star"},
    {60, "Wolf-Rayet NC Star"},
    {61, "Wolf-Rayet O Star"},
    {62, "Wolf-Rayet Star"},
    {63, "Y (Brown dwarf) Star"}
};

JsVolcanismType JsVolcanismType::instance {
    {1, "Carbon Dioxide Geysers"},
    {2, "Major Carbon Dioxide Geysers"},
    {3, "Major Metallic Magma"},
    {4, "Major Rocky Magma"},
    {5, "Major Silicate Vapour Geysers"},
    {6, "Major Water Geysers"},
    {7, "Major Water Magma"},
    {8, "Metallic Magma"},
    {9, "Minor Ammonia Magma"},
    {10, "Minor Carbon Dioxide Geysers"},
    {11, "Minor Metallic Magma"},
    {12, "Minor Methane Magma"},
    {13, "Minor Nitrogen Magma"},
    {14, "Minor Rocky Magma"},
    {15, "Minor Silicate Vapour Geysers"},
    {16, "Minor Water Geysers"},
    {17, "Minor Water Magma"},
    {18, "No volcanism"},
    {19, "Rocky Magma"},
    {20, "Silicate Vapour Geysers"},
    {21, "Water Geysers"},
    {22, "Water Magma"}
};

JsAtmosphereType JsAtmosphereType::instance {
    {1, "Ammonia"},
    {2, "Ammonia and Oxygen"},
    {3, "Ammonia-rich"},
    {4, "Argon"},
    {5, "Argon-rich"},
    {6, "Carbon dioxide"},
    {7, "Carbon dioxide-rich"},
    {8, "Helium"},
    {9, "Hot Argon"},
    {10, "Hot Argon-rich"},
    {11, "Hot Carbon dioxide"},
    {12, "Hot Carbon dioxide-rich"},
    {13, "Hot Metallic vapour"},
    {14, "Hot Silicate vapour"},
    {15, "Hot Sulphur dioxide"},
    {16, "Hot Water"},
    {17, "Hot Water-rich"},
    {18, "Hot thick Ammonia"},
    {19, "Hot thick Ammonia-rich"},
    {20, "Hot thick Argon"},
    {21, "Hot thick Argon-rich"},
    {22, "Hot thick Carbon dioxide"},
    {23, "Hot thick Carbon dioxide-rich"},
    {24, "Hot thick Metallic vapour"},
    {25, "Hot thick Methane"},
    {26, "Hot thick Methane-rich"},
    {27, "Hot thick Nitrogen"},
    {28, "Hot thick Silicate vapour"},
    {29, "Hot thick Sulphur dioxide"},
    {30, "Hot thick Water"},
    {31, "Hot thick Water-rich"},
    {32, "Hot thin Carbon dioxide"},
    {33, "Hot thin Metallic vapour"},
    {34, "Hot thin Silicate vapour"},
    {35, "Hot thin Sulphur dioxide"},
    {36, "Methane"},
    {37, "Methane-rich"},
    {38, "Neon"},
    {39, "Neon-rich"},
    {40, "Nitrogen"},
    {41, "No atmosphere"},
    {42, "Oxygen"},
    {43, "Suitable for water-based life"},
    {44, "Sulphur dioxide"},
    {45, "Thick Ammonia"},
    {46, "Thick Ammonia and Oxygen"},
    {47, "Thick Ammonia-rich"},
    {48, "Thick Argon"},
    {49, "Thick Argon-rich"},
    {50, "Thick Carbon dioxide"},
    {51, "Thick Carbon dioxide-rich"},
    {52, "Thick Helium"},
    {53, "Thick Methane"},
    {54, "Thick Methane-rich"},
    {55, "Thick Nitrogen"},
    {56, "Thick No atmosphere"},
    {57, "Thick Suitable for water-based life"},
    {58, "Thick Sulphur dioxide"},
    {59, "Thick Water"},
    {60, "Thick Water-rich"},
    {61, "Thin Ammonia"},
    {62, "Thin Ammonia and Oxygen"},
    {63, "Thin Ammonia-rich"},
    {64, "Thin Argon"},
    {65, "Thin Argon-rich"},
    {66, "Thin Carbon dioxide"},
    {67, "Thin Carbon dioxide-rich"},
    {68, "Thin Helium"},
    {69, "Thin Methane"},
    {70, "Thin Methane-rich"},
    {71, "Thin Neon"},
    {72, "Thin Neon-rich"},
    {73, "Thin Nitrogen"},
    {74, "Thin Oxygen"},
    {75, "Thin Sulphur dioxide"},
    {76, "Thin Water"},
    {77, "Thin Water-rich"},
    {78, "Water"},
    {79, "Water-rich"},

    {80, "Iron"},
    {81, "Silicates"},
    {82, "Hydrogen"},

};

JsSolidType JsSolidType::instance {
    {1, "Ice"},
    {2, "Metal"},
    {3, "Rock"},
};

JsTerraformingState JsTerraformingState::instance {
    {1, "Not terraformable"},
    {2, "Terraformable"},
    {3, "Terraforming"},
    {3, "Terraformed"},
};

JsMaterials JsMaterials::instance {
    {1, "Antimony"},
    {2, "Arsenic"},
    {3, "Cadmium"},
    {4, "Carbon"},
    {5, "Iron"},
    {6, "Nickel"},
    {7, "Niobium"},
    {8, "Phosphorus"},
    {9, "Sulphur"},
    {10, "Tellurium"},
    {11, "Tungsten"},
    {12, "Vanadium"},
    {13, "Zinc"},
    {14, "Zirconium"},
    {15, "Germanium"},
    {16, "Manganese"},
    {17, "Molybdenum"},
    {18, "Selenium"},
    {19, "Yttrium"},
    {20, "Ruthenium"},
    {21, "Chromium"},
    {22, "Tin"},
    {23, "Mercury"},
    {24, "Technetium"},
    {25, "Polonium"},


};

JsReserveLevel JsReserveLevel::instance {
    {1, "Depleted"},
    {2, "Low"},
    {3, "Common"},
    {4, "Major"},
    {5, "Pristine"},
};

JsTimestamps JsTimestamps::instance {
    {1, "distanceToArrival"},
    {2, "meanAnomaly"},
    {3, "ascendingNode"},
    {4, "controllingPower"},
    {5, "factions"},
    {6, "powerState"},
    {7, "powers"}
};

JsServices JsServices::instance {
    {1, "Flight Controller"},
    {2, "Station Operations"},
    {3, "Dock"},
    {4, "Autodock"},
    {5, "Station Menu"},
    {6, "Contacts"},
    {7, "Refuel"},
    {8, "Market"},
    {9, "Repair"},
    {10, "Workshop"},
    {11, "Missions"},
    {12, "Search and Rescue"},
    {13, "Missions Generated"},
    {14, "Restock"},
    {15, "Social Space"},
    {16, "Black Market"},
    {17, "Interstellar Factors Contact"},
    {18, "Powerplay"},
    {19, "System Colonisation"},
    {20, "Crew Lounge"},
    {21, "Universal Cartographics"},
    {22, "Outfitting"},
    {23, "Livery"},
    {24, "Construction Services"},
    {25, "Shop"},
    {26, "Bartender"},
    {27, "Vista Genomics"},
    {28, "Shipyard"},
    {29, "Pioneer Supplies"},
    {30, "Apex Interstellar"},
    {31, "Tuning"},
    {32, "Fleet Carrier Fuel"},
    {33, "Fleet Carrier Management"},
    {34, "Frontline Solutions"},
    {35, "Refinery Contact"},
    {36, "Redemption Office"},
    {37, "Technology Broker"},
    {38, "Material Trader"},
    {39, "Squadron Bank"},
    {40, "Fleet Carrier Administration"},
    {41, "On Dock Mission"},
    {42, "Fleet Carrier Vendor"},
};

JsStationType JsStationType::instance {
    {1, "Asteroid base"},
    {2, "Coriolis Starport"},
    {3, "Dockable Planet Station"},
    {4, "Dodec Starport"},
    {5, "Drake-Class Carrier"},
    {6, "Mega ship"},
    {7, "Ocellus Starport"},
    {8, "Orbis Starport"},
    {9, "Outpost"},
    {10, "Planetary Construction Depot"},
    {11, "Planetary Outpost"},
    {12, "Planetary Port"},
    {13, "Settlement"},
    {14, "Space Construction Depot"},
    {15, "Surface Settlement"},
};

JsStationState JsStationState::instance {
    {1, "Construction"},
    {2, "Damaged"},
    {3, "DamagedHuman"},
    {4, "UnderAttack"},
    {5, "UnderRepairs"},
};

JsCarrierDockingAccess JsCarrierDockingAccess::instance {
    {1, "None"},
    {2, "Friends"},
    {3, "Squadron"},
    {4, "Squadron Friends"},
    {5, "All"},
};


extern void test_json(StarSystemJS& ss_js);
void test() {
//    StarSystemJS ss_js;
//    test_json(ss_js);
}

void db::init_js_remapping() {
    auto& str_set = js::impl::gStrSet;

    str_set.insert({
        // signals genuses
        "$Codex_Ent_Aleoids_Genus_Name;",
        "$Codex_Ent_Bacterial_Genus_Name;",
        "$Codex_Ent_Brancae_Name;",
        "$Codex_Ent_Cactoid_Genus_Name;",
        "$Codex_Ent_Clypeus_Genus_Name;",
        "$Codex_Ent_Conchas_Genus_Name;",
        "$Codex_Ent_Cone_Name;",
        "$Codex_Ent_Electricae_Genus_Name;",
        "$Codex_Ent_Fonticulus_Genus_Name;",
        "$Codex_Ent_Fumerolas_Genus_Name;",
        "$Codex_Ent_Fungoids_Genus_Name;",
        "$Codex_Ent_Ground_Struct_Ice_Name;",
        "$Codex_Ent_Osseus_Genus_Name;",
        "$Codex_Ent_Recepta_Genus_Name;",
        "$Codex_Ent_Shrubs_Genus_Name;",
        "$Codex_Ent_Sphere_Name;",
        "$Codex_Ent_Stratum_Genus_Name;",
        "$Codex_Ent_Tubus_Genus_Name;",
        "$Codex_Ent_Tussocks_Genus_Name;",
        "$Codex_Ent_Tube_Name;",
        "$Codex_Ent_Vents_Name;",

        // ring/belt types
        "Icy",
        "Metal Rich",
        "Metallic",
        "Rocky",

        // ship types
        "Adder",
        "Alliance Challenger",
        "Alliance Chieftain",
        "Alliance Crusader",
        "Anaconda",
        "Asp Explorer",
        "Asp Scout",
        "Beluga Liner",
        "Caspian Explorer",
        "Cobra MkIII",
        "Cobra MkIV",
        "Cobra MkV",
        "Corsair",
        "Diamondback Explorer",
        "Diamondback Scout",
        "Dolphin",
        "Eagle",
        "Federal Assault Ship",
        "Federal Corvette",
        "Federal Dropship",
        "Federal Gunship",
        "Fer-de-Lance",
        "Hauler",
        "Imperial Clipper",
        "Imperial Courier",
        "Imperial Cutter",
        "Imperial Eagle",
        "Keelback",
        "Kestrel Mk II",
        "Krait MkII",
        "Krait Phantom",
        "Lynx Highliner",
        "Mamba",
        "Mandalay",
        "Orca",
        "Panther Clipper MkII",
        "Python",
        "Python MkII",
        "Sidewinder",
        "Type-10 Defender",
        "Type-11 Prospector",
        "Type-6 Transporter",
        "Type-7 Transporter",
        "Type-8 Transporter",
        "Type-9 Heavy",
        "Viper MkIII",
        "Viper MkIV",
        "Vulture",

        // outfitting categories
        "hardpoint",
        "internal",
        "utility",
        "standard",

        // outfitting modules
        "Power Distributor",
        "Fuel Scoop",
        "Sensors",
        "Life Support",
        "Shield Cell Bank",
        "Auto Field-Maintenance Unit",
        "Power Plant",
        "Shield Generator",
        "Thrusters",
        "Frame Shift Drive",
        "Frame Shift Drive (SCO)",
        "Lightweight Alloy",
        "Reinforced Alloy",
        "Refinery",
        "Military Grade Composite",
        "Prospector Limpet Controller",
        "Frame Shift Drive Interdictor",
        "Hatch Breaker Limpet Controller",
        "Fuel Transfer Limpet Controller",
        "Collector Limpet Controller",
        "Repair Limpet Controller",
        "Reactive Surface Composite",
        "Mirrored Surface Composite",
        "Cargo Rack",
        "Beam Laser",
        "Hull Reinforcement Package",
        "Module Reinforcement Package",
        "Pulse Laser",
        "Cannon",
        "Multi-Cannon",
        "Planetary Vehicle Hangar",
        "Burst Laser",
        "Fuel Tank",
        "Fragment Cannon",
        "Bi-Weave Shield Generator",
        "Shield Booster",
        "Sub-Surface Displacement Missile",
        "Mining Laser",
        "Recon Limpet Controller",
        "Seeker Missile Rack",
        "Cargo Scanner",
        "Kill Warrant Scanner",
        "Economy Class Passenger Cabin",
        "Frame Shift Wake Scanner",
        "Missile Rack",
        "Pulse Wave Analyser",
        "Torpedo Pylon",
        "Fighter Hangar",
        "Rail Gun",
        "Business Class Passenger Cabin",
        "Prismatic Shield Generator",
        "Plasma Accelerator",
        "Mine Launcher",
        "Abrasion Blaster",
        "Xeno Multi Limpet Controller",
        "Mk II Economy Class Passenger Cabin",
        "Mk II Cargo Rack",
        "Seismic Charge Launcher",
        "Mining Multi Limpet Controller",
        "First Class Passenger Cabin",
        "Operations Multi Limpet Controller",
        "Rescue Multi Limpet Controller",
        "Decontamination Limpet Controller",
        "Mk II Business Class Passenger Cabin",
        "Detailed Surface Scanner",
        "Chaff Launcher",
        "Heat Sink Launcher",
        "Standard Docking Computer",
        "Supercruise Assist",
        "Research Limpet Controller",
        "Electronic Countermeasure",
        "Advanced Docking Computer",
        "Shock Mine Launcher",
        "Advanced Planetary Approach Suite",
        "AX Multi-Cannon",
        "AX Missile Rack",
        "Meta Alloy Hull Reinforcement",
        "Planetary Approach Suite",
        "Mk II Mining Multi-Limpet Controller",
        "Luxury Class Passenger Cabin",
        "Mk II Plasma Shock Accelerator",
        "Point Defence",
        "Mining Volley Repeater",
        "Enhanced AX Multi-Cannon",
        "Mk II Supercharge Optimised Frame Shift Drive (SCO)",
        "Remote Release Flak Launcher",
        "Mk II Agile Boost Thrusters",
        "Mk II Gravity Optimised Thrusters",
        "Mk II Vessel Hangar",
        "Universal Multi Limpet Controller",
        "Xeno Scanner",
        "Mk II Ablative Lightweight Alloys",
        "Mk II Ablative Reinforced Alloys",
        "Mk II Ablative Military Grade Composite",
        "Mk II Ablative Reactive Surface Composite",
        "Mk II Ablative Mirrored Surface Composite",
        "Shutdown Field Neutraliser",
        "Pack-Hound Missile Rack",
        "Imperial Hammer Rail Gun",
        "Enforcer Cannon",
        "Advanced Plasma Accelerator",
        "Pulse Disruptor Laser",
        "Retributor Beam Laser",
        "Concord Cannon",
        "Rocket Propelled FSD Disruptor",
        "Pacifier Frag-Cannon",
        "Mining Lance",
        "Cytoscrambler Burst Laser",
        "Thargoid Pulse Neutraliser",
        "Enhanced Xeno Scanner",
        "Advanced Multi-Cannon",
        "Shock Cannon",
        "Enzyme Missile Rack",
        "Advanced Missile Rack",
        "Enhanced AX Missile Rack",
        "Corrosion Resistant Cargo Rack",
        "Remote Release Flechette Launcher",
        "Guardian Shield Reinforcement",
        "Experimental Weapon Stabiliser",
        "Guardian Hull Reinforcement",
        "Guardian Module Reinforcement",
        "Guardian Hybrid Power Distributor",
        "Guardian Hybrid Power Plant",
        "Guardian Shard Cannon",
        "Guardian Plasma Charger",
        "Guardian FSD Booster",
        "Caustic Sink Launcher",
        "Pulse Wave Xeno Scanner",
        "Guardian Nanite Torpedo Pylon",
        "Sub-Surface Extraction Missile",
        "Guardian Gauss Cannon",
        "Enhanced Performance Thrusters",
        "Advanced Discovery Scanner",
        "Intermediate Discovery Scanner",
        "Basic Discovery Scanner",

        // commodity categories
        "Chemicals",
        "Consumer Items",
        "Foods",
        "Industrial Materials",
        "Legal Drugs",
        "Machinery",
        "Medicines",
        "Metals",
        "Minerals",
        "Salvage",
        "Slavery",
        "Technology",
        "Textiles",
        "Waste",
        "Weapons",
    });
    test();
}

JsEnumDecl::JsEnumDecl(std::string_view nm, IList values) {
    name = nm;
    allValues.reserve(values.size());
    for (auto it = values.begin(); it != values.end(); it++)
        allValues.emplace_back(new JsEnumVal{it->first, std::string(it->second), this});
    mapById.reserve(allValues.size());
    mapByName.reserve(allValues.size());
    for (auto &v: allValues) {
        mapById.insert({v->id, v.get()});
        mapByName.insert({v->str, v.get()});
        js::impl::gStrSet.insert(v->str);
    }
}

JsEnumVal *JsEnumDecl::get(unsigned id) const {
    if (auto it = mapById.find(id); it != mapById.end())
        return it->second;
    return nullptr;
}

JsEnumVal *JsEnumDecl::get(std::string_view sv) const {
    if (auto it = mapByName.find(std::string(sv)); it != mapByName.end())
        return it->second;
    return nullptr;
}

JsEnumVal* JsEnumDecl::addNewValue(const std::string& str) {
    LOG_ERROR("Error: Added new key \"{}\" into enum '{}'", str, name);
    auto& v = allValues.emplace_back(new JsEnumVal{0, str, this});
    for (auto &v: allValues) {
        mapById.insert({v->id, v.get()});
        mapByName.insert({v->str, v.get()});
        js::impl::gStrSet.insert(v->str);
    }
    return v.get();
}


} // namespace db