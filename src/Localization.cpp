#include "Localization.h"
#include "globals.h"        // APIDefs
#include "DecoderRingApi.h"
#include "GW2API.h"

#include <mutex>
#include <array>
#include <cstring>
#include <unordered_map>

namespace Localization {
namespace {

    // CL admits any Decoder Ring whose ABI is at least this. Resolve() (item
    // names) has existed since v1; the current live service is v4. This is
    // deliberately NOT DECODER_RING_API_VERSION, so recopying a newer header
    // never silently drops an older-but-usable service.
    constexpr uint32_t CL_DECODER_MIN_VERSION = 4u;
    constexpr uint32_t CL_DECODER_LANG_VERSION = 5u;   // GetActiveLanguage() added here

    const char* const SENTINEL = "CL_ActiveLanguageProbe";

    std::mutex s_mutex;
    std::unordered_map<uint32_t, std::string> s_itemNames;  // id -> localized (per active lang)
    std::unordered_map<uint32_t, std::string> s_itemDescs;  // id -> localized description (v3+)
    std::string s_lang = "en";
    bool s_subscribed = false;

    // Map a Nexus 2-letter language identifier to a language we support; else "en".
    const char* MapNexusToApi(const char* code) {
        if (!code) return "en";
        if (!std::strcmp(code, "de")) return "de";
        if (!std::strcmp(code, "fr")) return "fr";
        if (!std::strcmp(code, "es")) return "es";
        return "en";
    }

    // Re-validate on EVERY use; never cache the pointer (DR can unload any time).
    DecoderRingApi* GetDecoder() {
        if (!APIDefs || !APIDefs->DataLink_Get) return nullptr;
        auto* api = static_cast<DecoderRingApi*>(APIDefs->DataLink_Get(DECODER_RING_DATALINK));
        return (api && api->apiVersion >= CL_DECODER_MIN_VERSION) ? api : nullptr;
    }

    // Compute the current effective language: prefer DR's resolved language,
    // else the Nexus sentinel probe. Returns an immortal literal.
    const char* DetectLang() {
        DecoderRingApi* dr = GetDecoder();
        if (dr && dr->apiVersion >= CL_DECODER_LANG_VERSION && dr->GetActiveLanguage) {
            const char* l = dr->GetActiveLanguage();
            return MapNexusToApi(l);
        }
        if (APIDefs && APIDefs->Localization_Translate) {
            const char* l = APIDefs->Localization_Translate(SENTINEL);
            return MapNexusToApi(l);
        }
        return "en";
    }

    void ApplyLang(const char* lang) {
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (s_lang == lang) return;
            s_lang = lang;
            s_itemNames.clear();   // localized caches are per-language
            s_itemDescs.clear();
        }
        // Refresh currency names for the new language (achievement names are
        // fetched on demand by the prereq display path). English needs no fetch —
        // the embedded names are already English.
        if (std::strcmp(lang, "en") != 0)
            CraftyLegend::GW2API::FetchCurrencyNamesAsync(lang);
    }

    // Decoder Ring resolved a record in the background — cache item names.
    void OnResolved(void* payload) {
        if (!payload) return;
        const DecoderRecord* rec = static_cast<const DecoderRecord*>(payload);
        if (rec->status != DR_Resolved) return;
        if (rec->linkType != 0x02) return;         // items only
        std::lock_guard<std::mutex> lock(s_mutex);
        if (rec->name[0] != '\0') s_itemNames[rec->id] = rec->name;
        if (rec->schemaVersion >= 3 && rec->description[0] != '\0')
            s_itemDescs[rec->id] = rec->description;
    }

    void OnDecoderReady(void*)     { /* presence is re-checked per call via GetDecoder() */ }
    void OnDecoderUnloading(void*) { /* keep cached names; they remain valid */ }

    void OnLanguageChanged(void* payload) {
        if (payload) ApplyLang(MapNexusToApi(static_cast<const char*>(payload)));
        else ApplyLang(DetectLang());
    }

    // --- UI chrome table. Key = English source string. Columns: de, fr, es.
    // Missing key or missing column -> English (the key) is shown. Extend freely;
    // untranslated strings simply stay English.
    using Row = std::array<const char*, 3>; // {de, fr, es}
    const std::unordered_map<std::string, Row>& ChromeTable() {
        static const std::unordered_map<std::string, Row> t = {
            {"Shopping List",   {"Einkaufsliste", "Liste d'achats", "Lista de compra"}},
            // Column titles
            {"Details",         {"Details", "Détails", "Detalles"}},
            {"Mystic Forge",    {"Mystische Schmiede", "Forge mystique", "Fragua mística"}},
            {"Acquisition Methods", {"Beschaffungsmethoden", "Méthodes d'acquisition", "Métodos de adquisición"}},
            {"Crafting Materials", {"Handwerksmaterialien", "Matériaux d'artisanat", "Materiales de artesanía"}},
            {"Craft",           {"Herstellen", "Fabriquer", "Fabricar"}},
            {"attempts",        {"Versuche", "essais", "intentos"}},
            {"Show Shopping List", {"Einkaufsliste anzeigen", "Afficher la liste d'achats", "Mostrar lista de compra"}},
            {"Hide Shopping List", {"Einkaufsliste ausblenden", "Masquer la liste d'achats", "Ocultar lista de compra"}},
            {"Prerequisites",   {"Voraussetzungen", "Prérequis", "Requisitos"}},
            {"Acquisition",     {"Beschaffung", "Acquisition", "Adquisición"}},
            {"Materials",       {"Materialien", "Matériaux", "Materiales"}},
            {"Legendaries",     {"Legendäre", "Légendaires", "Legendarios"}},
            {"Favourites",      {"Favoriten", "Favoris", "Favoritos"}},
            {"Weapons",         {"Waffen", "Armes", "Armas"}},
            {"Armour",          {"Rüstung", "Armure", "Armadura"}},
            {"Armor",           {"Rüstung", "Armure", "Armadura"}},
            {"Trinkets",        {"Schmuck", "Bijoux", "Abalorios"}},
            {"Search",          {"Suche", "Rechercher", "Buscar"}},
            {"Owned",           {"Im Besitz", "Possédé", "En posesión"}},
            {"Complete",        {"Abgeschlossen", "Terminé", "Completado"}},
            {"Settings",        {"Einstellungen", "Paramètres", "Ajustes"}},
            {"Show item icons", {"Item-Symbole anzeigen", "Afficher les icônes d'objet", "Mostrar iconos de objeto"}},
            {"Show Quick Access Icon", {"Schnellzugriffssymbol anzeigen", "Afficher l'icône d'accès rapide", "Mostrar icono de acceso rápido"}},
            {"Use Pie UI theme (if available)", {"Pie-UI-Design verwenden (falls verfügbar)", "Utiliser le thème Pie UI (si disponible)", "Usar tema Pie UI (si está disponible)"}},
            {"Refresh",         {"Aktualisieren", "Actualiser", "Actualizar"}},
            {"Close",           {"Schließen", "Fermer", "Cerrar"}},
            {"Total",           {"Gesamt", "Total", "Total"}},
            {"Vendor",          {"Händler", "Vendeur", "Vendedor"}},
            {"Trading Post",    {"Handelsposten", "Comptoir", "Puesto comercial"}},
            {"No account data", {"Keine Kontodaten", "Aucune donnée de compte", "Sin datos de cuenta"}},
            {"Wallet Currency", {"Guthaben-Währung", "Monnaie du porte-monnaie", "Moneda del monedero"}},
            // Item rarity tiers (GW2 terms)
            {"Junk",       {"Plunder", "Camelote", "Chatarra"}},
            {"Basic",      {"Einfach", "Basique", "Básico"}},
            {"Fine",       {"Fein", "Raffiné", "Fino"}},
            {"Masterwork", {"Meisterwerk", "Chef-d'œuvre", "Obra maestra"}},
            {"Rare",       {"Selten", "Rare", "Excepcional"}},
            {"Exotic",     {"Exotisch", "Exotique", "Exótico"}},
            {"Ascended",   {"Aufgestiegen", "Élevé", "Ascendido"}},
            {"Legendary",  {"Legendär", "Légendaire", "Legendario"}},
            // Item subtypes shown beside legendary names (weapon/armor/trinket). Weapons/trinkets
            // are standard GW2 terms; armor slot/weight forms are best-effort (native review).
            {"sword",      {"Schwert", "Épée", "Espada"}},
            {"greatsword", {"Großschwert", "Espadon", "Mandoble"}},
            {"dagger",     {"Dolch", "Dague", "Daga"}},
            {"axe",        {"Axt", "Hache", "Hacha"}},
            {"Axe",        {"Axt", "Hache", "Hacha"}},
            {"mace",       {"Streitkolben", "Masse", "Maza"}},
            {"hammer",     {"Hammer", "Marteau", "Martillo"}},
            {"shield",     {"Schild", "Bouclier", "Escudo"}},
            {"focus",      {"Fokus", "Focus", "Foco"}},
            {"torch",      {"Fackel", "Torche", "Antorcha"}},
            {"warhorn",    {"Kriegshorn", "Cor de guerre", "Cuerno de guerra"}},
            {"scepter",    {"Zepter", "Sceptre", "Cetro"}},
            {"staff",      {"Stab", "Bâton", "Bastón"}},
            {"longbow",    {"Langbogen", "Arc long", "Arco largo"}},
            {"shortbow",   {"Kurzbogen", "Arc court", "Arco corto"}},
            {"rifle",      {"Gewehr", "Fusil", "Rifle"}},
            {"pistol",     {"Pistole", "Pistolet", "Pistola"}},
            {"trident",    {"Dreizack", "Trident", "Tridente"}},
            {"Speargun",   {"Speergewehr", "Harpon", "Fusil de arpón"}},
            {"Harpoon",    {"Harpune", "Harpon", "Arpón"}},
            {"Spear",      {"Speer", "Lance", "Lanza"}},
            {"Spear/Staff", {"Speer/Stab", "Lance/Bâton", "Lanza/Bastón"}},
            {"Accessory",  {"Accessoire", "Accessoire", "Accesorio"}},
            {"Ring",       {"Ring", "Anneau", "Anillo"}},
            {"Amulet",     {"Amulett", "Amulette", "Amuleto"}},
            {"Relic",      {"Relikt", "Relique", "Reliquia"}},
            {"Backpiece",  {"Rückenteil", "Objet de dos", "Objeto de espalda"}},
            {"Helm",       {"Helm", "Casque", "Casco"}},
            {"Shoulders",  {"Schultern", "Épaulières", "Hombreras"}},
            {"Coat",       {"Wams", "Plastron", "Sobretúnica"}},
            {"Gloves",     {"Handschuhe", "Gants", "Guantes"}},
            {"Leggings",   {"Beinkleid", "Jambières", "Calzas"}},
            {"Boots",      {"Stiefel", "Bottes", "Botas"}},
            {"Aquabreather", {"Atemgerät", "Masque de plongée", "Respirador acuático"}},
            {"Heavy Helm",      {"Schwerer Helm", "Casque lourd", "Casco pesado"}},
            {"Heavy Shoulders", {"Schwere Schultern", "Épaulières lourdes", "Hombreras pesadas"}},
            {"Heavy Coat",      {"Schweres Wams", "Plastron lourd", "Sobretúnica pesada"}},
            {"Heavy Gloves",    {"Schwere Handschuhe", "Gants lourds", "Guantes pesados"}},
            {"Heavy Leggings",  {"Schweres Beinkleid", "Jambières lourdes", "Calzas pesadas"}},
            {"Heavy Boots",     {"Schwere Stiefel", "Bottes lourdes", "Botas pesadas"}},
            {"Medium Helm",      {"Mittlerer Helm", "Casque moyen", "Casco medio"}},
            {"Medium Shoulders", {"Mittlere Schultern", "Épaulières moyennes", "Hombreras medias"}},
            {"Medium Coat",      {"Mittleres Wams", "Plastron moyen", "Sobretúnica media"}},
            {"Medium Gloves",    {"Mittlere Handschuhe", "Gants moyens", "Guantes medios"}},
            {"Medium Leggings",  {"Mittleres Beinkleid", "Jambières moyennes", "Calzas medias"}},
            {"Medium Boots",     {"Mittlere Stiefel", "Bottes moyennes", "Botas medias"}},
            {"Light Helm",      {"Leichter Helm", "Casque léger", "Casco ligero"}},
            {"Light Shoulders", {"Leichte Schultern", "Épaulières légères", "Hombreras ligeras"}},
            {"Light Coat",      {"Leichtes Wams", "Plastron léger", "Sobretúnica ligera"}},
            {"Light Gloves",    {"Leichte Handschuhe", "Gants légers", "Guantes ligeros"}},
            {"Light Leggings",  {"Leichtes Beinkleid", "Jambières légères", "Calzas ligeras"}},
            {"Light Boots",     {"Leichte Stiefel", "Bottes légères", "Botas ligeras"}},
            // Best-effort pass — short labels/headers/buttons (PROVISIONAL, pending native review)
            {"Legendary Items", {"Legendäre Gegenstände", "Objets légendaires", "Objetos legendarios"}},
            {"No legendary items loaded.", {"Keine legendären Gegenstände geladen.", "Aucun objet légendaire chargé.", "No se han cargado objetos legendarios."}},
            {"Name",            {"Name", "Nom", "Nombre"}},
            {"Price",           {"Preis", "Prix", "Precio"}},
            {"Gold Cost",       {"Goldkosten", "Coût en or", "Coste en oro"}},
            {"Total TP: ",      {"Handelsposten gesamt: ", "Total comptoir : ", "Total puesto: "}},
            {"Total Vendor: ",  {"Händler gesamt: ", "Total vendeur : ", "Total vendedor: "}},
            {"Open on Wiki",    {"Im Wiki öffnen", "Ouvrir sur le wiki", "Abrir en la wiki"}},
            {"Add Favourite",   {"Favorit hinzufügen", "Ajouter aux favoris", "Añadir a favoritos"}},
            {"Remove Favourite", {"Favorit entfernen", "Retirer des favoris", "Quitar de favoritos"}},
            {"Search in Hoard & Seek", {"In Hoard & Seek suchen", "Rechercher dans Hoard & Seek", "Buscar en Hoard & Seek"}},
            {"CraftyLegend Settings", {"CraftyLegend-Einstellungen", "Paramètres de CraftyLegend", "Ajustes de CraftyLegend"}},
            {"Display Settings", {"Anzeigeeinstellungen", "Paramètres d'affichage", "Ajustes de pantalla"}},
            {"Compact Mode",    {"Kompaktmodus", "Mode compact", "Modo compacto"}},
            {"Owned legendaries", {"Besessene Legendäre", "Légendaires possédés", "Legendarios en posesión"}},
            {"Show all",        {"Alle anzeigen", "Tout afficher", "Mostrar todos"}},
            {"Hide once I own one", {"Ausblenden, sobald ich eines besitze", "Masquer dès que j'en possède un", "Ocultar cuando tenga uno"}},
            {"Hide only when armoury is full", {"Nur bei voller Waffenkammer ausblenden", "Masquer seulement si l'arsenal est plein", "Ocultar solo si la armería está llena"}},
            {"Update Account Data", {"Kontodaten aktualisieren", "Actualiser les données du compte", "Actualizar datos de cuenta"}},
            {"Refresh TP Prices", {"TP-Preise aktualisieren", "Actualiser les prix du comptoir", "Actualizar precios del puesto"}},
            {"Refresh crafting levels", {"Handwerksstufen aktualisieren", "Actualiser les niveaux d'artisanat", "Actualizar niveles de artesanía"}},
            {"Re-reads each character's crafting disciplines from the API. Normally refreshed once a day.", {"Liest die Handwerksdisziplinen jedes Charakters erneut über die API. Wird normalerweise einmal täglich aktualisiert.", "Relit les disciplines d'artisanat de chaque personnage via l'API. Actualisé une fois par jour normalement.", "Vuelve a leer las disciplinas de artesanía de cada personaje desde la API. Normalmente se actualiza una vez al día."}},
            {"Homepage",        {"Startseite", "Site web", "Página web"}},
            {"Buy me a coffee!", {"Spendier mir einen Kaffee!", "Offre-moi un café !", "¡Invítame a un café!"}},
            // Short H&S status lines
            {"Hoard & Seek addon not found", {"Hoard & Seek-Addon nicht gefunden", "Extension Hoard & Seek introuvable", "Complemento Hoard & Seek no encontrado"}},
            {"Looking for Hoard & Seek...", {"Suche nach Hoard & Seek...", "Recherche de Hoard & Seek...", "Buscando Hoard & Seek..."}},
            {"Waiting for Hoard & Seek...", {"Warte auf Hoard & Seek...", "En attente de Hoard & Seek...", "Esperando a Hoard & Seek..."}},
            {"Waiting for Hoard & Seek permission...", {"Warte auf Hoard & Seek-Berechtigung...", "En attente de l'autorisation de Hoard & Seek...", "Esperando permiso de Hoard & Seek..."}},
            {"Hoard & Seek permission denied", {"Hoard & Seek-Berechtigung verweigert", "Autorisation Hoard & Seek refusée", "Permiso de Hoard & Seek denegado"}},
            {"(Hoard & Seek connected)", {"(Hoard & Seek verbunden)", "(Hoard & Seek connecté)", "(Hoard & Seek conectado)"}},
            {"(cached prices loaded)", {"(zwischengespeicherte Preise geladen)", "(prix en cache chargés)", "(precios en caché cargados)"}},
            // Prerequisite names — non-Achievement categories. Map names are AUTHORITATIVE
            // GW2 /v2/maps localized names; the descriptor prefix is best-effort. Masteries
            // (proper GW2 names) are left English pending native review.
            {"World Completion", {"Welterkundung", "Exploration du monde", "Exploración del mundo"}},
            {"Verdant Brink Map Completion", {"Kartenerkundung: Grasgrüne Schwelle", "Exploration de carte : Orée d'émeraude", "Exploración del mapa: Umbral Verdeante"}},
            {"Auric Basin Map Completion", {"Kartenerkundung: Güldener Talkessel", "Exploration de carte : Bassin aurique", "Exploración del mapa: Valle Áurico"}},
            {"Tangled Depths Map Completion", {"Kartenerkundung: Verschlungene Tiefen", "Exploration de carte : Profondeurs verdoyantes", "Exploración del mapa: Profundidades Enredadas"}},
            {"Dragon's Stand Map Completion", {"Kartenerkundung: Widerstand des Drachen", "Exploration de carte : Repli du dragon", "Exploración del mapa: Defensa del Dragón"}},
            {"Seitung Province Map Completion", {"Kartenerkundung: Provinz Seitung", "Exploration de carte : Province de Seitung", "Exploración del mapa: Provincia de Seitung"}},
            {"New Kaineng City Map Completion", {"Kartenerkundung: Stadt Neu-Kaineng", "Exploration de carte : Néo-Kaineng", "Exploración del mapa: Ciudad de Nueva Kaineng"}},
            {"The Echovald Wilds Map Completion", {"Kartenerkundung: Die Echowald-Wildnis", "Exploration de carte : Terres sauvages d'Echovald", "Exploración del mapa: Las Selvas de Echovald"}},
            {"Dragon's End Map Completion", {"Kartenerkundung: Drachen-Ende", "Exploration de carte : Trépas du dragon", "Exploración del mapa: Muerte del Dragón"}},
            {"Skywatch Archipelago Map Completion", {"Kartenerkundung: Himmelswacht-Archipel", "Exploration de carte : Archipel de l'observatoire céleste", "Exploración del mapa: Archipiélago Vistacielo"}},
            {"Amnytas Map Completion", {"Kartenerkundung: Amnytas", "Exploration de carte : Amnytas", "Exploración del mapa: Amnytas"}},
            {"Inner Nayos Map Completion", {"Kartenerkundung: Inneres Nayos", "Exploration de carte : Nayos intérieur", "Exploración del mapa: Zona interior de Nayos"}},
            {"Lowland Shore Map Completion", {"Kartenerkundung: Tiefland-Küste", "Exploration de carte : Côte des basses terres", "Exploración del mapa: Costas Bajas"}},
            {"Janthir Syntri Map Completion", {"Kartenerkundung: Janthir Syntri", "Exploration de carte : Syntri de Janthir", "Exploración del mapa: Janthir Syntri"}},
            {"Bava Nisos Map Completion", {"Kartenerkundung: Bava Nisos", "Exploration de carte : Bava Nisos", "Exploración del mapa: Bava Nisos"}},
            {"Mistburned Barrens Map Completion", {"Kartenerkundung: Nebelbrand-Ödland", "Exploration de carte : Landes de Feu-de-Brume", "Exploración del mapa: Yermos de Pavesas de Niebla"}},
            {"Ursus Oblivium Map Completion", {"Kartenerkundung: Ursus Oblivium", "Exploration de carte : Ursus Oblivium", "Exploración del mapa: Ursus Oblivium"}},
            {"Shipwreck Strand", {"Strand der Wracks", "Rive aux épaves", "Litoral del Naufragio"}},
            {"Starlit Weald", {"Sternenlicht-Gehölz", "Bois étoilé", "Bosquezuelo Estelar"}},
            {"Crystal Oasis: Trade Contracts", {"Kristalloase: Handelsverträge", "Oasis de cristal : contrats commerciaux", "Oasis de Cristal: contratos comerciales"}},
            {"Desert Highlands: Trade Contracts", {"Wüsten-Hochland: Handelsverträge", "Hautes-terres du désert : contrats commerciaux", "Tierras Altas del Desierto: contratos comerciales"}},
            {"Elon Riverlands: Trade Contracts", {"Elon-Flusslande: Handelsverträge", "Rives de l'Elon : contrats commerciaux", "La Ribera del Elon: contratos comerciales"}},
            {"The Desolation: Trade Contracts", {"Das Ödland: Handelsverträge", "La Désolation : contrats commerciaux", "La Desolación: contratos comerciales"}},
            {"Gift of Battle Reward Track", {"Belohnungspfad: Geschenk der Schlacht", "Voie de récompense : Don de bataille", "Vía de recompensa: Don de batalla"}},
            {"Triumphant Armor Reward Track", {"Belohnungspfad: Triumphierende Rüstung", "Voie de récompense : Armure triomphante", "Vía de recompensa: Armadura triunfal"}},
            {"Salvage Ascended Equipment", {"Aufgestiegene Ausrüstung bergen", "Recycler l'équipement élevé", "Reciclar equipo ascendido"}},
            {"Acquiring Agaleus", {"Agaleus erwerben", "Acquérir Agaleus", "Adquirir Agaleus"}},
            // Masteries — authoritative GW2 /v2/masteries localized names
            {"Itzel Lore: Adrenal Mushrooms", {"Lehre der Itzel: Adrenalpilze", "Ethnologie des Itzels: Champignons surrénaux", "Conocimientos itzel: Champiñones adrenalínicos"}},
            {"Nuhoch Training: Nuhoch Alchemy", {"Lehre der Nuhoch: Alchemie der Nuhoch", "Ethnologie des Nuhochs: Alchimie nuhoch", "Conocimientos nuhoch: Alquimia nuhoch"}},
            {"Exalted Lore: Exalted Gathering", {"Lehre der Erhabenen: Erhabenes Sammeln", "Ethnologie des Exaltés: Récolte exaltée", "Conocimientos exaltados: Recolección exaltada"}},
            {"Gliding: Ley Line Gliding", {"Gleiten: Ley-Linien-Gleiten", "Vol en deltaplane: Vol dans les lignes de force", "Planeo: Uso de energía de líneas ley"}},
            {"Mount Masteries", {"Reit-Meisterschaften", "Maîtrises de monture", "Maestrías de montura"}},
            // Prerequisite category headers
            {"Map Completion",  {"Kartenerkundung", "Exploration de carte", "Exploración del mapa"}},
            {"Masteries",       {"Meisterschaften", "Maîtrises", "Maestrías"}},
            {"World vs World",  {"Welt gegen Welt", "Monde contre Monde", "Mundo contra Mundo"}},
            {"Collections",     {"Sammlungen", "Collections", "Colecciones"}},
            {"Achievements",    {"Erfolge", "Succès", "Logros"}},
            {"Salvage",         {"Bergung", "Récupération", "Recuperación"}},
            {"Map Currencies",  {"Kartenwährungen", "Monnaies de carte", "Monedas del mapa"}},
            {"Other",           {"Sonstiges", "Autre", "Otro"}},
            // Achievement-gate tooltip tags
            {"Requires achievement", {"Erfordert Erfolg", "Nécessite un succès", "Requiere logro"}},
            {"Achievement complete", {"Erfolg abgeschlossen", "Succès terminé", "Logro completado"}},
            {"Locked - requires achievement", {"Gesperrt – erfordert Erfolg", "Verrouillé – nécessite un succès", "Bloqueado: requiere logro"}},
            {"Progress", {"Fortschritt", "Progression", "Progreso"}},
            // Crafting discipline tooltip (PROVISIONAL, pending native review)
            {"Can craft now", {"Kann jetzt herstellen", "Peut fabriquer maintenant", "Puede fabricar ahora"}},
            {"Needs a discipline swap", {"Benötigt Disziplinwechsel", "Nécessite un changement de discipline", "Requiere cambio de disciplina"}},
            {"No character can craft this yet", {"Noch kein Charakter kann dies herstellen", "Aucun personnage ne peut encore fabriquer ceci", "Ningún personaje puede fabricar esto todavía"}},
            {"Closest", {"Am nächsten", "Au plus près", "Más cerca"}},
            {"at", {"auf", "à", "a"}},
            {"or", {"oder", "ou", "o"}},
            {"any rating", {"beliebige Stufe", "niveau quelconque", "cualquier nivel"}},
            {"Loading character data...", {"Charakterdaten werden geladen...", "Chargement des données de personnage...", "Cargando datos de personaje..."}},
            {"Character data unavailable", {"Charakterdaten nicht verfügbar", "Données de personnage indisponibles", "Datos de personaje no disponibles"}},
            {"Your API key needs the 'characters' permission", {"Ihr API-Schlüssel benötigt die Berechtigung 'characters'", "Votre clé API nécessite l'autorisation 'characters'", "Tu clave de API necesita el permiso 'characters'"}},
            {"Hoard & Seek denied Crafty Legend permission", {"Hoard & Seek hat Crafty Legend die Berechtigung verweigert", "Hoard & Seek a refusé l'autorisation à Crafty Legend", "Hoard & Seek denegó el permiso a Crafty Legend"}},
            {"Approve it in Hoard & Seek's settings, then use 'Refresh crafting levels'", {"Erlauben Sie es in den Einstellungen von Hoard & Seek und nutzen Sie dann 'Handwerksstufen aktualisieren'", "Autorisez-le dans les paramètres de Hoard & Seek, puis utilisez 'Actualiser les niveaux d'artisanat'", "Autorízalo en los ajustes de Hoard & Seek y luego usa 'Actualizar niveles de artesanía'"}},
            {"Your Hoard & Seek is too old for character data", {"Ihr Hoard & Seek ist zu alt für Charakterdaten", "Votre Hoard & Seek est trop ancien pour les données de personnage", "Tu Hoard & Seek es demasiado antiguo para los datos de personaje"}},
            {"Update Hoard & Seek and relaunch the game", {"Aktualisieren Sie Hoard & Seek und starten Sie das Spiel neu", "Mettez Hoard & Seek à jour et relancez le jeu", "Actualiza Hoard & Seek y reinicia el juego"}},
        };
        return t;
    }

    int LangCol(const std::string& lang) {
        if (lang == "de") return 0;
        if (lang == "fr") return 1;
        if (lang == "es") return 2;
        return -1; // en -> use the English key
    }

} // namespace

void Init() {
    if (!APIDefs) return;

    // Register the language sentinel: its value in each language IS that
    // language's code, so Localization_Translate(SENTINEL) reveals the active
    // Nexus language (Nexus exposes no direct getter). Fallback path only.
    if (APIDefs->Localization_Set) {
        APIDefs->Localization_Set(SENTINEL, "en", "en");
        APIDefs->Localization_Set(SENTINEL, "de", "de");
        APIDefs->Localization_Set(SENTINEL, "fr", "fr");
        APIDefs->Localization_Set(SENTINEL, "es", "es");
    }

    if (APIDefs->Events_Subscribe) {
        APIDefs->Events_Subscribe(EV_DECODER_RING_READY,            OnDecoderReady);
        APIDefs->Events_Subscribe(EV_DECODER_RING_UNLOADING,        OnDecoderUnloading);
        APIDefs->Events_Subscribe(EV_DECODER_RING_RESOLVED,         OnResolved);
        APIDefs->Events_Subscribe(EV_DECODER_RING_LANGUAGE_CHANGED, OnLanguageChanged);
        s_subscribed = true;
    }
    // DR may have loaded first: prompt it to (re-)announce readiness.
    if (APIDefs->Events_Raise)
        APIDefs->Events_Raise(EV_DECODER_RING_PING, nullptr);

    ApplyLang(DetectLang());

    if (APIDefs && APIDefs->Log)
        APIDefs->Log(LOGL_INFO, "CraftyLegend", DiagStatus().c_str());
}

void Shutdown() {
    if (APIDefs && s_subscribed && APIDefs->Events_Unsubscribe) {
        APIDefs->Events_Unsubscribe(EV_DECODER_RING_READY,            OnDecoderReady);
        APIDefs->Events_Unsubscribe(EV_DECODER_RING_UNLOADING,        OnDecoderUnloading);
        APIDefs->Events_Unsubscribe(EV_DECODER_RING_RESOLVED,         OnResolved);
        APIDefs->Events_Unsubscribe(EV_DECODER_RING_LANGUAGE_CHANGED, OnLanguageChanged);
    }
    s_subscribed = false;
}

void Poll() {
    ApplyLang(DetectLang());
}

bool DecoderPresent() {
    return GetDecoder() != nullptr;
}

const char* ActiveLang() {
    std::lock_guard<std::mutex> lock(s_mutex);
    // Return an immortal literal, not s_lang.c_str() (which could dangle).
    if (s_lang == "de") return "de";
    if (s_lang == "fr") return "fr";
    if (s_lang == "es") return "es";
    return "en";
}

std::string ItemName(uint32_t id, const std::string& englishFallback) {
    if (id == 0) return englishFallback;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_itemNames.find(id);
        if (it != s_itemNames.end()) return it->second;
    }
    // English clients: DR would just return English anyway — skip the round trip.
    if (LangCol(ActiveLang()) < 0) return englishFallback;

    DecoderRingApi* dr = GetDecoder();
    if (!dr) return englishFallback;

    DecoderRecord rec{};
    DecoderStatus st = dr->Resolve(0x02, id, nullptr, &rec);
    if (st == DR_Resolved && rec.name[0] != '\0') {
        std::string name = rec.name;
        std::lock_guard<std::mutex> lock(s_mutex);
        s_itemNames[id] = name;
        if (rec.schemaVersion >= 3 && rec.description[0] != '\0')
            s_itemDescs[id] = rec.description;   // capture description in the same resolve
        return name;
    }
    // NotReady -> background fetch kicked; OnResolved fills the cache next frame.
    return englishFallback;
}

std::string ItemDescription(uint32_t id, const std::string& englishFallback) {
    if (id == 0) return englishFallback;
    if (LangCol(ActiveLang()) < 0) return englishFallback;   // English -> embedded text
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto d = s_itemDescs.find(id);
        if (d != s_itemDescs.end()) return d->second;
        if (s_itemNames.count(id)) return englishFallback;   // already resolved, DR had no description
    }
    DecoderRingApi* dr = GetDecoder();
    if (!dr) return englishFallback;

    DecoderRecord rec{};
    DecoderStatus st = dr->Resolve(0x02, id, nullptr, &rec);
    if (st == DR_Resolved) {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (rec.name[0] != '\0') s_itemNames[id] = rec.name;
        if (rec.schemaVersion >= 3 && rec.description[0] != '\0') {
            s_itemDescs[id] = rec.description;
            return s_itemDescs[id];
        }
    }
    // NotReady/Failed, or resolved-but-no-description -> English fallback.
    return englishFallback;
}

std::string ColumnTitle(const std::string& title) {
    if (title.empty()) return title;
    if (LangCol(ActiveLang()) < 0) return title;   // English

    // Exact match against the chrome table (static titles like "Mystic Forge").
    std::string t = Tr(title.c_str());
    if (t != title) return t;

    auto starts = [&](const char* p) { return title.rfind(p, 0) == 0; };

    // "Craft (WPN 500)" -> localize "Craft", keep discipline code + rating.
    if (starts("Craft (")) return std::string(Tr("Craft")) + title.substr(5);
    // "Vendor - Dugan" -> localize "Vendor", keep the vendor name.
    if (starts("Vendor - ")) return std::string(Tr("Vendor")) + title.substr(6);
    // "Mystic Forge (~5 attempts)" -> localize the words, keep the number.
    if (starts("Mystic Forge (~")) {
        std::string suffix = title.substr(std::string("Mystic Forge").size());
        const std::string att = "attempts";
        size_t pos = suffix.find(att);
        if (pos != std::string::npos) suffix.replace(pos, att.size(), Tr("attempts"));
        return std::string(Tr("Mystic Forge")) + suffix;
    }
    // "<item> - Acquisition" -> keep the (proper) item name, localize the suffix.
    const std::string acq = " - Acquisition";
    if (title.size() >= acq.size() && title.compare(title.size() - acq.size(), acq.size(), acq) == 0)
        return title.substr(0, title.size() - acq.size()) + " - " + Tr("Acquisition");

    return title; // free-text acquisition description -> English fallback
}

std::string DiagStatus() {
    std::string s = "i18n: lang=";
    s += ActiveLang();
    uint32_t ver = 0;
    bool getter = false;
    if (APIDefs && APIDefs->DataLink_Get) {
        auto* api = static_cast<DecoderRingApi*>(APIDefs->DataLink_Get(DECODER_RING_DATALINK));
        if (api) {
            ver = api->apiVersion;
            if (ver >= CL_DECODER_LANG_VERSION && api->GetActiveLanguage) getter = true;
        }
    }
    s += "  DecoderRing=";
    s += ver ? ("v" + std::to_string(ver)) : std::string("absent");
    if (ver && ver < CL_DECODER_MIN_VERSION) s += " (below min, treated absent)";
    if (getter) s += " +getter";
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s += "  cachedNames=" + std::to_string(s_itemNames.size());
    }
    s += "  " + CraftyLegend::GW2API::LocalizedDiag();
    return s;
}

const char* Tr(const char* key) {
    if (!key) return "";
    int col = LangCol(ActiveLang());
    if (col < 0) return key;                       // English
    const auto& t = ChromeTable();
    auto it = t.find(key);
    if (it == t.end()) return key;                 // untranslated -> English
    const char* v = it->second[col];
    return (v && v[0]) ? v : key;
}

} // namespace Localization
