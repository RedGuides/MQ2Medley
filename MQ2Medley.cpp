// MQ2Medley.cpp - Bard song scheduling plugin for MacroQuest2
//
// Changelog
//   2015-07-08 Winnower - Initial release
//   2016-06-22 Dewey    - Updated for current core MQ source: bump to v 1.01
//							- fixed using instant cast AA's
//							- fixed TLO ${Medley.Tune} so it does not CTD
//							- fixed TLO ${Medley.Medley} so it returns correct set.
//							- /medley quiet is now a lot more quiet.
//   2016-06-22 Dewey    - Updated for current core MQ source: bump to v 1.02
//							- fixed recast calculations due to core changes to TLO ${Spell[].CastTime}
//							- /medley debug now toggles some extra debug spam.
//   2016-07-10 Dewey    - Added SongIF which works like MQ2Melee SkillIF[] : bump to v 1.03
//							- /medley debug now saves its state and shows which song it is evaluating.
//   2016-07-11 Dewey    - Medley is now much more persistent, like priority version of MQ2Melee. Bump to v1.04
//							- Any time  Medley set or Twist changes state it should write Medley=[SET] and Playing=[0|1] to the ini.
//							- Sitting, Hover, Death and FD no longer turn off twist, instead they pause playback.
//   2016-07-17 Dewey	 - Modified Once/Queue'd spells now check to see if they are ready to cast. Bump to v1.05
//							-Modified profile loading.Should no longer spam NULL songs when loading empty profiles.
//   2016-07-18 Dewey	 - Modified once so it shares the same spell list.  v1.06
//							- ${Medley.getTimeTillQueueEmpty} broken.
//							- Experimental support for dynamic target from profile supporting mezing XTarget[n]
//   2016-07-26 Dewey	 - Updated for the eqMules MANDITORY STRING SAFETY DANCE.  v1.07
//
//
//-----------------
// MQ2Twist contributers:
//    koad 03-24-04 Original plugin (http://macroquest.sourceforge.net/phpBB2/viewtopic.php?t=5962&start=2)
//    CyberTech 03-31-04 w/ code/ideas from Falco72 & Space-boy
//    Cr4zyb4rd 08-19-04 taking over janitorial duties
//    Pheph 08-24-04 cleaning up use of MQ2Data
//    Simkin 12-17-07 Updated for Secrets of Faydwer 10 Songs
//    MinosDis 05-01-08 Updated to fix /twist once
//    dewey2461 09-19-09 Updated to let you twist AA abilities and define clicky/AA songs from inside the game
//    htw 09-22-09+ See changes below
//    gSe7eN 04-03-12 Fixed Show to dShow for March 31st build

/*
MQ2Medley

Usage:
/medley name - Sing the given medley
/medley queue "song/item/aa name" [-targetid|spawnid] [-interupt] - add songs to queue to cast once
/medley stop/end/off - stop singing
/medley - Resume the medley after using /medley stop
/medley delay # - 10ths of a second, minimum of 0, default 3, how long after casting a spell to wait to cast next spell
/medley reload - reload the INI file
/medley quiet - Toggles songs listing for medley and queued songs

----------------------------
Item Click Method:
MQ2Medley uses /cast item "ItemName"

----------------------------
Examples:
/medley melee
play medley defined in [MQ2Medley-melee] ini setion
/medley queue "Dirge of the Sleepwalker" -interupt
Interrupt current song and cast AA "Dirge of the Sleepwaler"
/medley queue "Slumber of Silisia" -targetid|${Me.XTarget[2].ID}
When current song ends, will mez XTarget[2] and switch back current target.
Target will be switched for one pulse, which is typicaly less than 20ms
/medley queue "Blade of Vesagran"
Add epic click to queue
/medley queue "Lesson of the Devoted"
Lesson of the Devoted AA will be added to the twist queue and sung when current song finished

----------------------------
MQ2Data TLO Variables:
Members:
Medley.Medley
- string of current medley
- false (boolean) if no current medley
Medley.TTQE (time to queue empty)
- double time in seconds until queue is empty, this is estimate only.  If performating normal medley, this will be 0.0
Medley.Tune
- int 1 if buffed with "A Tune Stuck in My Head", 0 otherwise
Medley.Active
- boolean true if MQ2Medley is currently trying to cast spells
----------------------------

The ini file has the format:
[MQ2Medley]
Delay=3       Delay between twists in 1/10th of second. Lag & System dependant.
[MQ2Medley-medleyname]   can multiple one of these sections, for each medley you define
songIF=Condition to turn entire block on/off
song1=Name of Song/Item/AA^expression representing duration of song^condition expression for this song to be song
...
song20=

*/

#include <mq/Plugin.h>

PreSetup("MQ2Medley");
PLUGIN_VERSION(1.07);

constexpr int MAX_MEDLEY_SIZE = 30;

class SongData
{
public:
	enum SpellType {
		SONG = 1,
		ITEM = 2,
		AA = 3,
		NOT_FOUND = 4
	};

	std::string name;
	SpellType type;
	bool once;					// is this a cast once spell?
	int castTime;				// 10ths of second
	unsigned long targetID;		// SpawnID
	std::string durationExp;	// duration in seconds, how long the spell lasts, evaluated with Math.Calc
	std::string conditionalExp; // condition to cast this song under, evaluated with Math.Calc
	std::string targetExp;		// expression for targetID
public:
	SongData(std::string spellName, SpellType spellType, int spellCastTime);

	~SongData();

	bool isReady();  // true if spell/item/aa is ready to cast (no timer)
	double evalDuration();
	bool evalCondition();
	DWORD evalTarget();
};

const SongData nullSong = SongData("", SongData::NOT_FOUND, -1);
//long TUNE_SPELL_ID = GetSpellByName("A Tune Stuck in Your Head")->ID;

bool MQ2MedleyEnabled = false;
long CAST_PAD_TIME = 3;                  // in 10ths of seconds, to give spell time to finish
std::list<SongData> medley;                // medley[n] = stores medley list
std::string medleyName;

std::map<std::string, __int64 > songExpires;   // when cast, songExpires["songName"] = GetTime() * SongDurationSeconds*10
									  // song to song state variables
SongData currentSong = nullSong;
boolean bWasInterrupted = false;
__int64 CastDue = 0;
PSPAWNINFO TargetSave = NULL;

bool bTwist = false;

bool quiet = false;
bool DebugMode = false;
bool Initialized = false;
char SongIF[MAX_STRING] = "";


void resetTwistData()
{
	medley.clear();
	medleyName = "";

	currentSong = nullSong;
	bWasInterrupted = false;

	bTwist = false;
	SongIF[0] = 0;
	WritePrivateProfileString("MQ2Medley", "Playing", "0", INIFileName);
	WritePrivateProfileString("MQ2Medley", "Medley", "", INIFileName);
}

//get current timestamp in tenths of a second
__int64 GetTime()
{
	return (__int64)(MQGetTickCount64() / 100);
}

// returns time in seconds till queust is empty. precision 10ths of second
double getTimeTillQueueEmpty()
{
	return 0;
	/*
	double time = 0.0;
	if ((bCurrentSongAlt || !altQueue.empty()) && GetTime() < CastDue) {
	time += (CastDue - GetTime()) / 10.0;
	}

	for (auto song = altQueue.begin(); song != altQueue.end(); song++) {
	time += CAST_PAD_TIME / 10.0;
	time += song->castTime / 10.0;
	}

	return time;
	*/
}

void Evaluate(char *zOutput, char *zFormat, ...) {
	//char zOutput[MAX_STRING]={0};
	va_list vaList;
	va_start(vaList, zFormat);
	vsprintf_s(zOutput, MAX_STRING, zFormat, vaList);
	//DebugSpewAlways("E[%s]",zOutput);
	ParseMacroData(zOutput,MAX_STRING);
	//DebugSpewAlways("R[%s]",zOutput);
	//WriteChatf("::: zOutput = [ %s ]",zOutput);
}


// -1 if not found
// cast time in 10th of seconds if found
// Note: CastTime TLO changed now returns milliseconds so take result / 100 to get 10ths of a sec
long GetItemCastTime(std::string ItemName)
{
	char zOutput[MAX_STRING] = { 0 };
	sprintf_s(zOutput, "${FindItem[=%s].CastTime}", ItemName.c_str());
	ParseMacroData(zOutput, MAX_STRING);
	DebugSpew("MQ2Medley::GetItemCastTime ${FindItem[=%s].CastTime} returned=%s", ItemName.c_str(), zOutput);
	if (!_stricmp(zOutput, "null"))
		return -1;

	return (long)(atof(zOutput) / 100);
}

// -1 if not found
// cast time in 10th of seconds if found
long GetAACastTime(std::string AAName)
{
	char zOutput[MAX_STRING] = { 0 };
	sprintf_s(zOutput, "${Me.AltAbility[%s].Spell.CastTime}", AAName.c_str());
	ParseMacroData(zOutput, MAX_STRING);
	DebugSpew("MQ2Medley::GetAACastTime ${Me.AltAbility[%s].Spell.CastTime} returned=%s", AAName.c_str(), zOutput);
	if (!_stricmp(zOutput, "null"))
		return -1;

	return (long)(atof(zOutput) / 100);
}

void MQ2MedleyDoCommand(PSPAWNINFO pChar, PCHAR szLine)
{
	DebugSpew("MQ2Medley::MQ2MedleyDoCommand(pChar, %s)", szLine);
	HideDoCommand(pChar, szLine, FromPlugin);
}

int GemCastTime(const std::string& spellName) // Gem 1 to NUM_SPELL_GEMS
{
	VePointer<CONTENTS> n;
	for (int i = 0; i < NUM_SPELL_GEMS; i++)
	{
		PSPELL pSpell = GetSpellByID(GetPcProfile()->MemorizedSpells[i]);
		if (pSpell && spellName.compare(pSpell->Name) == 0) {
			float mct = (FLOAT)(GetCastingTimeModifier((EQ_Spell*)pSpell) + GetFocusCastingTimeModifier((EQ_Spell*)pSpell, n, 0) + pSpell->CastTime) / 1000.0f;
			if (mct < 0.50 * pSpell->CastTime / 1000.0f)
				return (int)(0.50 * (pSpell->CastTime / 100.0f));
			else
				return (int)(mct * 10);
		}
	}

	return -1;
}

/**
* Get the current casting spell and store in szCurrentCastingSpell, if failed to udpate this will return false
*/
//bool getCurrentCastingSpell()
//{
//	DebugSpew("MQ2Medley::getCurrentCastingSpell -ENTER ");
//	PCSIDLWND pCastingWindow = (PCSIDLWND)pCastingWnd;
//	if (!pCastingWindow->IsVisible())
//	{
//		DebugSpew("MQ2Medley::getCurrentCastingSpell - not active", szCurrentCastingSpell);
//		return false;
//	}
//	if (CXWnd *Child = ((CXWnd*)pCastingWnd)->GetChildItem("Casting_SpellName")) {
//		if (GetCXStr(Child->WindowText, szCurrentCastingSpell, 2047) && szCurrentCastingSpell[0] != '\0') {
//			DebugSpew("MQ2Medley::getCurrentCastingSpell -- current cast is: ", szCurrentCastingSpell);
//			return true;
//		}
//	}
//	return false;
//}


SongData getSongData(const char* name)
{
	std::string spellName = name;  // gem spell, item, or AA

	                          // if spell name is a # convert to name for that gem
	const int spellNum = (int)strtol(name, NULL, 10);   // will return 0 on the strings
	if (spellNum>0 && spellNum <= NUM_SPELL_GEMS) {
		DebugSpew("MQ2Medley::TwistCommand Parsing gem %d", spellNum);
		PSPELL pSpell = GetSpellByID(GetPcProfile()->MemorizedSpells[spellNum - 1]);
		if (pSpell) {
			spellName = pSpell->Name;
		}
		else {
			WriteChatf("\arMQ2Medley\au::\arInvalid spell number specified (\ay%s\ar) - ignoring.", name);
			return nullSong;
		}
	}

	int castTime = GemCastTime(spellName);
	if (castTime >= 0)
	{
		return SongData(spellName, SongData::SONG, castTime);
	}

	castTime = GetItemCastTime(spellName);
	if (castTime >= 0)
	{
		return SongData(spellName, SongData::ITEM, castTime);
	}

	castTime = GetAACastTime(spellName);
	if (castTime >= 0)
	{
		return SongData(spellName, SongData::AA, castTime);
	}

	return nullSong;
}

// returns time it will take to cast (in 1/10 seconds)
// preconditions:
//   SongTodo is ready to cast
// -1 - cast failed
int doCast(const SongData SongTodo)
{
	DebugSpew("MQ2Medley::doCast(%s) ENTER", SongTodo.name.c_str());
	//WriteChatf("MQ2Medley::doCast(%s) ENTER", SongTodo.name.c_str());
	char szTemp[MAX_STRING] = { 0 };
	if (GetCharInfo())
	{
		if (GetCharInfo()->pSpawn)
		{
			switch (SongTodo.type) {
			case SongData::SONG:
				for (int i = 0; i < NUM_SPELL_GEMS; i++)
				{
					PSPELL pSpell = GetSpellByID(GetPcProfile()->MemorizedSpells[i]);
					if (pSpell && SongTodo.name.compare(pSpell->Name) == 0) {
						int gemNum = i + 1;

						if (!SongTodo.targetID) {
							// do nothing special
						}
						else if (PSPAWNINFO Target = (PSPAWNINFO)GetSpawnByID(SongTodo.targetID)) {
							TargetSave = pTarget;
							pTarget = Target;
							DebugSpew("MQ2Medley::doCast - Set target to %d", Target->SpawnID);
						}
						else {
							WriteChatf("MQ2Medley::doCast - cannot find targetID=%d for to cast \"%s\", SKIPPING", SongTodo.targetID, SongTodo.name.c_str());
							return -1;
						}

						sprintf_s(szTemp, "/multiline ; /stopsong ; /cast %d", gemNum);
						MQ2MedleyDoCommand(GetCharInfo()->pSpawn, szTemp);

						return SongTodo.castTime;
					}
				}
				WriteChatf("MQ2Medley::doCast - could not find \"%s\" to cast, SKIPPING", SongTodo.name.c_str());

				return -1;
				break;
			case SongData::ITEM:
				DebugSpew("MQ2Medley::doCast - Next Song (Casting Item  \"%s\")", SongTodo.name.c_str());
				sprintf_s(szTemp, "/multiline ; /stopsong ; /cast item \"%s\"", SongTodo.name.c_str());
				MQ2MedleyDoCommand(GetCharInfo()->pSpawn, szTemp);
				return SongTodo.castTime;
			case SongData::AA:
				DebugSpew("MQ2Medley::doCast - Next Song (Casting AA  \"%s\")", SongTodo.name.c_str());
				sprintf_s(szTemp, "/multiline ; /stopsong ; /alt act ${Me.AltAbility[%s].ID}", SongTodo.name.c_str());
				MQ2MedleyDoCommand(GetCharInfo()->pSpawn, szTemp);
				return SongTodo.castTime;
			default:
				// This is the null song - do nothing.
				WriteChatf("MQ2Medley::doCast - unsupported type %d for \"%s\", SKIPPING", SongTodo.type, SongTodo.name.c_str());
				return -1; // todo
			}
		}
	}
	return -1;
}

template <unsigned int _Size>LPSTR SafeItoa(int _Value, char(&_Buffer)[_Size], int _Radix)
{
	errno_t err = _itoa_s(_Value, _Buffer, _Radix);
	if (!err) {
		return _Buffer;
	}
	return "";
}

void Update_INIFileName(PCHARINFO pCharInfo) {
	sprintf_s(INIFileName, "%s\\%s_%s.ini", gPathConfig, EQADDR_SERVERNAME, pCharInfo->Name);
}

void Load_MQ2Medley_INI_Medley(PCHARINFO pCharInfo, std::string medleyName);
void Load_MQ2Medley_INI(PCHARINFO pCharInfo)
{
	char szTemp[MAX_STRING] = { 0 };
	char szKey[MAX_STRING] = { 0 };


	Update_INIFileName(pCharInfo);

	CAST_PAD_TIME = GetPrivateProfileInt("MQ2Medley", "Delay", 3, INIFileName);
	WritePrivateProfileString("MQ2Medley", "Delay", SafeItoa(CAST_PAD_TIME, szTemp, 10), INIFileName);
	quiet = GetPrivateProfileInt("MQ2Medley", "Quiet", 0, INIFileName) ? 1 : 0;
	WritePrivateProfileString("MQ2Medley", "Quiet", SafeItoa(quiet, szTemp, 10), INIFileName);
	DebugMode = GetPrivateProfileInt("MQ2Medley", "Debug", 1, INIFileName) ? 1 : 0;
	WritePrivateProfileString("MQ2Medley", "Debug", SafeItoa(DebugMode, szTemp, 10), INIFileName);
	GetPrivateProfileString("MQ2Medley", "Medley", "", szTemp, MAX_STRING, INIFileName);
	if (szTemp[0] != 0)
	{
		Load_MQ2Medley_INI_Medley(pCharInfo, szTemp);
		bTwist = GetPrivateProfileInt("MQ2Medley", "Playing", 1, INIFileName) ? 1 : 0;
	}
}

void Load_MQ2Medley_INI_Medley(PCHARINFO pCharInfo, std::string medleyName)
{
	char szTemp[MAX_STRING] = { 0 };
	char *pNext;

	Update_INIFileName(pCharInfo);

	std::string iniSection = "MQ2Medley-" + medleyName;
	for (int i = 0; i < MAX_MEDLEY_SIZE; i++)
	{
		std::string iniKey = "song" + std::to_string(i + 1);
		if (GetPrivateProfileString(iniSection.c_str(), iniKey.c_str(), "", szTemp, MAX_STRING, INIFileName))
		{
			SongData medleySong = nullSong;

			//ugly ass split logic, example: song1=War March of Jocelyn^180.0^${Melee.Combat}
			char *p = strtok_s(szTemp, "^", &pNext);
			if (p)
			{
				medleySong = getSongData(p);
				if (medleySong.type == SongData::NOT_FOUND) {
					WriteChatf("MQ2Medley::loadMedley - could not find song named \"%s\"", p);
					continue;
				}
				if (p = strtok_s(NULL, "^",&pNext))
				{
					medleySong.durationExp = p;
					if (p = strtok_s(NULL, "^", &pNext))
					{
						medleySong.conditionalExp = p;
						if (p = strtok_s(NULL, "^", &pNext))
						{
							medleySong.targetExp = p;
						}
					}
				}
			}

			if (medleySong.type != SongData::NOT_FOUND)
			{
				if (!quiet) WriteChatf("MQ2Medley::loadMedley - adding Song %s^%s^s", medleySong.name.c_str(), medleySong.durationExp.c_str(), medleySong.conditionalExp.c_str());
				medley.emplace_back(medleySong);
			}
		}
	}
	WriteChatf("MQ2Medley::loadMedley - %d song Medley loaded", (int)medley.size());
	GetPrivateProfileString(iniSection.c_str(), "SongIF", "", SongIF, MAX_STRING, INIFileName);
}


void StopTwistCommand(PSPAWNINFO pChar, PCHAR szLine)
{
	char szTemp[MAX_STRING] = { 0 };
	GetArg(szTemp, szLine, 1);
	bTwist = false;
	currentSong = nullSong;
	MQ2MedleyDoCommand(pChar, "/stopsong");
	if (_strnicmp(szTemp, "silent", 6))
		WriteChatf("\arMQ2Medley\au::\atStopping Medley");
	WritePrivateProfileString("MQ2Medley", "Playing", SafeItoa(bTwist, szTemp, 10), INIFileName);
}



void DisplayMedleyHelp() {
	WriteChatf("\arMQ2Medley \au- \atSong Scheduler - read documentation online");
}

// **************************************************  *************************
// Function:      MedleyCommand
// Description:   Our /medley command. schedule songs to sing
// **************************************************  *************************
void MedleyCommand(PSPAWNINFO pChar, PCHAR szLine)
{
	char szTemp[MAX_STRING] = { 0 }, szTemp1[MAX_STRING] = { 0 };
	char szMsg[MAX_STRING] = { 0 };
	char szChat[MAX_STRING] = { 0 };

	int argNum = 1;
	GetArg(szTemp, szLine, argNum);

	if ((!medley.empty() && (!strlen(szTemp)) || !_strnicmp(szTemp, "start", 5))) {
		GetArg(szTemp1, szLine, 2);
		if (_strnicmp(szTemp1, "silent", 6))
			WriteChatf("\arMQ2Medley\au::\atStarting Twist.");
		bTwist = true;
		CastDue = -1;
		WritePrivateProfileString("MQ2Medley", "Playing", SafeItoa(bTwist, szTemp, 10), INIFileName);
		return;
	}

	if (!_strnicmp(szTemp, "debug", 5)) {
		DebugMode = !DebugMode;
		WriteChatf("\arMQ2Medley\au::\atDebug mode is now %s\ax.", DebugMode ? "\ayON" : "\agOFF");
		WritePrivateProfileString("MQ2Medley", "Debug", SafeItoa(DebugMode, szTemp, 10), INIFileName);
		return;
	}

	if (!_strnicmp(szTemp, "stop", 4) || !_strnicmp(szTemp, "end", 3) || !_strnicmp(szTemp, "off", 3)) {
		GetArg(szTemp1, szLine, 2);
		if (_strnicmp(szTemp1, "silent", 6))
			StopTwistCommand(pChar, szTemp);
		else
			StopTwistCommand(pChar, szTemp1);
		return;
	}

	if (!_strnicmp(szTemp, "reload", 6) || !_strnicmp(szTemp, "load", 4)) {
		WriteChatf("\arMQ2Medley\au::\atReloading INI Values.");
		Load_MQ2Medley_INI(GetCharInfo());
		return;
	}

	if (!_strnicmp(szTemp, "delay", 5)) {
		GetArg(szTemp, szLine, 2);
		if (strlen(szTemp)) {
			int delay = atoi(szTemp);
			if (delay < 0)
			{
				WriteChatf("\arMQ2Medley\au::\ayWARNING: \arDelay cannot be less than 0, setting to 0");
				delay = 0;
			}
			CAST_PAD_TIME = delay;
			Update_INIFileName(GetCharInfo());
			WritePrivateProfileString("MQ2Medley", "Delay", SafeItoa(CAST_PAD_TIME, szTemp, 10), INIFileName);
			WriteChatf("\arMQ2Medley\au::\atSet delay to \ag%d\at, INI updated.", CAST_PAD_TIME);
		}
		else
			WriteChatf("\arMQ2Medley\au::\atDelay \ag%d\at.", CAST_PAD_TIME);
		return;
	}

	if (!_strnicmp(szTemp, "quiet", 5)) {
		quiet = !quiet;
		WritePrivateProfileString("MQ2Medley", "Quiet", SafeItoa(quiet, szTemp, 10), INIFileName);
		WriteChatf("\arMQ2Medley\au::\atNow being %s\at.", quiet ? "\ayquiet" : "\agnoisy");
		return;
	}

	if (!_strnicmp(szTemp, "clear", 5)) {
		resetTwistData();
		StopTwistCommand(pChar, szTemp);
		if (!quiet)
			WriteChatf("\arMQ2Medley\au::\ayMedley Cleared.");
		return;
	}

	// check help arg, or display if we have no songs defined and /twist was used
	if (!strlen(szTemp) || !_strnicmp(szTemp, "help", 4)) {
		DisplayMedleyHelp();
		return;
	}

	//boolean isQueue = false;
	//boolean isInterrupt = true;

	if (!_strnicmp(szTemp, "queue", 4) || !_strnicmp(szTemp, "once", 4)) {
		WriteChatf("\arMQ2Medley\au::\ayAdding to once queue");
		argNum++;

		GetArg(szTemp, szLine, argNum++);
		if (!strlen(szTemp)) {
			WriteChatf("\arMQ2Medley\au::\atqueue requires spell/item/aa to cast", szTemp);
			return;
		}
		SongData songData = getSongData(szTemp);
		if (songData.type == SongData::NOT_FOUND) {
			WriteChatf("\arMQ2Medley\au::\atUnable to find spell for \"%s\", skipping", szTemp);
			return;
		}

		do {
			GetArg(szTemp, szLine, argNum++);
			if (szTemp[0] == 0) {
				break;
			}
			else if (!_strnicmp(szTemp, "-targetid|", 10)) {
				songData.targetID = atoi(&szTemp[10]);
				DebugSpew("MQ2Medley::TwistCommand  - queue \"%s\" targetid=%d", songData.name.c_str(), songData.targetID);
			}
			else if (!_strnicmp(szTemp, "-interrupt", 10)) {
				currentSong = nullSong;
				CastDue = -1;
				MQ2MedleyDoCommand(pChar, "/stopsong");
			}

		} while (true);
		songData.once = 1;

		DebugSpew("MQ2Medley::TwistCommand  - altQueue.push_back(%s);", songData.name.c_str());
		//altQueue.push_back(songData);
		medley.push_front(songData);
		return;
	}

	//if (!_strnicmp(szTemp, "improv", 6)) {
	//	argNum++;
	//	WriteChatf("\arMQ2Medley\au::\atImprov Medley.");
	//	medley.clear();
	//	medleyName = "improv";

	//	while (true) {
	//		GetArg(szTemp, szLine, argNum);
	//		argNum++;
	//		if (!strlen(szTemp))
	//			break;

	//		DebugSpew("MQ2Medley::TwistCommand Parsing song %s", szTemp);

	//		SongData songData = getSongData(szTemp);
	//		if (songData.type == SongData::NOT_FOUND) {
	//			WriteChatf("\arMQ2Medley\au::\atUnable to find spell for \"%s\", skipping", szTemp);
	//			continue;
	//		}

	//		DebugSpew("MQ2Medley::TwistCommand  - medley.emplace_back.(%s);", songData.name.c_str());
	//		medley.emplace_back(songData);

	//	}

	//	if (!quiet)
	//		WriteChatf("\arMQ2Medley\au::\atTwisting \ag%d \atsong%s.", medley.size(), medley.size()>1 ? "s" : "");

	//	if (medley.size()>0)
	//		bTwist = true;
	//	if (isInterrupt)
	//	{
	//		currentSong = nullSong;
	//		CastDue = -1;
	//		MQ2MedleyDoCommand(pChar, "/stopsong");
	//	}

	//}

	if (strlen(szTemp)) {
		WriteChatf("\arMQ2Medley\au::\atLoading medley \"%s\"", szTemp);
		medley.clear();
		medleyName = szTemp;
		WritePrivateProfileString("MQ2Medley", "Medley", szTemp, INIFileName);
		Load_MQ2Medley_INI_Medley(GetCharInfo(), medleyName);
		bTwist = true;
		WritePrivateProfileString("MQ2Medley", "Playing", SafeItoa(bTwist, szTemp, 10), INIFileName);
		return;
	}
	else if (!medley.empty()) {
		WriteChatf("\arMQ2Medley\au::\atResuming medley \"%s\"", medleyName.c_str());
		bTwist = true;
		WritePrivateProfileString("MQ2Medley", "Playing", SafeItoa(bTwist, szTemp, 10), INIFileName);
	}
	else {
		WriteChatf("\arMQ2Medley\au::\atNo medley defined");
	}
}

/*
Checks to see if character is in a fit state to cast next song/item

Note 1: Do not try to correct SIT state, or you will have to stop the
twist before re-memming songs

Note 2: Since the auto-stand-on-cast bullcrap added to EQ a few patches ago,
chars would stand up every time it tried to twist a medley.  So now
we stop twisting at sit.
*/
bool CheckCharState()
{
	if (!bTwist)
		return false;

	if (GetCharInfo()) {
		if (!GetCharInfo()->pSpawn)
			return false;
		if (GetCharInfo()->Stunned == 1)
			return false;
		switch (GetCharInfo()->standstate) {
		case STANDSTATE_SIT:
			//WriteChatf("\arMQ2Medley\au::\ayStopping Twist.");
			//bTwist = false;
			return false;
		case STANDSTATE_FEIGN:
			MQ2MedleyDoCommand(GetCharInfo()->pSpawn, "/stand");
			return false;
		case STANDSTATE_DEAD:
			WriteChatf("\arMQ2Medley\au::\ayStopping Twist.");
			//bTwist = false;
			return false;
		default:
			break;
		}
		if (InHoverState()) {
			//bTwist = false;
			return false;
		}
	}

	return true;
}

class MQ2MedleyType *pMedleyType = 0;

class MQ2MedleyType : public MQ2Type
{
private:
	char szTemp[MAX_STRING];
public:
	enum MedleyMembers {
		Medley = 1,
		TTQE = 2,
		Tune = 3,
		Active
	};

	MQ2MedleyType() :MQ2Type("Medley") {
		TypeMember(Medley);
		TypeMember(TTQE);
		TypeMember(Tune);
		TypeMember(Active);
	}

	~MQ2MedleyType() {}

	virtual bool GetMember(MQVarPtr VarPtr, const char* Member, char* Index, MQTypeVar& Dest) override {
		MQTypeMember* pMember = MQ2MedleyType::FindMember(Member);
		if (!pMember)
			return false;
		switch (pMember->ID) {
			case Medley:
				/* Returns: string
				current medley name
				empty string if no current medly
				*/
				sprintf_s(szTemp, "%s", medleyName.c_str());
				//medleyName.copy(szTemp, MAX_STRING);
				Dest.Ptr = szTemp;
				Dest.Type = mq::datatypes::pStringType;
				return true;
			case TTQE:
				/* Returns: double
				0 - if nothing is queued and performing normal medley
				#.# - double estimate time till queue is completed
				*/
				Dest.Double = getTimeTillQueueEmpty();
				Dest.Type = mq::datatypes::pDoubleType;
				return true;
			case Tune:
				/* Returns: int
				0 - not buffed with A Tune Stuck in My Head
				1 - buffed with A Tune Stuck in My Head
				*/
				Dest.Int = 0;
				Dest.Type = mq::datatypes::pIntType;
				char zOutput[MAX_STRING];
				sprintf_s(zOutput, "${Me.Buff[Tune Stuck In Your Head].ID}");
				ParseMacroData(zOutput,MAX_STRING);
				Dest.Int = atof(zOutput) != 0.0;
				return true;
			case Active:
				/* Returns: boolean
				true - medley is active
				*/
				Dest.Int = bTwist;
				Dest.Type = mq::datatypes::pBoolType;
				return true;
			default:
				break;
		}
		return false;
	}

	bool ToString(MQVarPtr VarPtr, char* Destination) override
	{
		strcpy_s(Destination, MAX_STRING, bTwist ? "TRUE" : "FALSE");
		return true;
	}
};

bool dataMedley(const char* szIndex, MQTypeVar& Dest)
{
	Dest.DWord = 1;
	Dest.Type = pMedleyType;
	return true;
}


// ******************************
// **** MQ2 API Calls Follow ****
// ******************************

PLUGIN_API void InitializePlugin()
{
	DebugSpewAlways("Initializing MQ2Medley");
	AddCommand("/medley", MedleyCommand, 0, 1, 1);
	AddMQ2Data("Medley", dataMedley);
	pMedleyType = new MQ2MedleyType;
	WriteChatf("\atMQ2Medley \agv%1.2f \ax loaded.", MQ2Version);
}

PLUGIN_API void ShutdownPlugin()
{
	DebugSpewAlways("MQ2Medley::Shutting down");
	RemoveCommand("/medley");
	RemoveMQ2Data("Medley");
	delete pMedleyType;
}

const SongData scheduleNextSong()
{
	__int64 currentTick = GetTime();  // 10th of seconds
	if (DebugMode) WriteChatf("MQ2Medley::scheduleNextSong - currentTick=%I64d", currentTick);
	SongData * stalestSong = NULL;
	for (auto song = medley.begin(); song != medley.end(); song++)
	{
		if (!song->isReady()) {
			DebugSpew("MQ2Medley::scheduleNextSong skipping[%s] (not ready)", song->name.c_str());
			continue;
		}
		if (!song->evalCondition()) {
			DebugSpew("MQ2Medley::scheduleNextSong skipping[%s] (condition not met)", song->name.c_str());
			continue;
		}

		if (song->once) {
			SongData nextSong = *song;
			medley.erase(song);
			return nextSong;
		}

		if (!stalestSong)
			stalestSong = &(*song);

		// for a 3s casting time song, we should recast if it will expire in the next 6 seconds
		// the constant 3 seconds is we will assume if we don't cast this song now, the next song will probably be a 3
		// second cast time song
		long long startCastBy = songExpires[song->name] - song->castTime - 30;
		if (DebugMode) WriteChatf("MQ2Medley::scheduleNextSong time till need to cast %s: %I64d 10th sec", song->name.c_str(), startCastBy - currentTick);

		// for a 3s casting time song, we should recast if it will expire in the next 6 seconds
		// the constant 3 seconds is we will assume if we don't cast this song now, the next song will probably be a 3
		// second cast time song
		if (startCastBy  < currentTick)
			return *song;

		if (songExpires[song->name] < songExpires[stalestSong->name])
			stalestSong = &(*song);
	}

	// we didn't find a song that had priority to cast, so we'll cast the song that will expirest instead
	if (stalestSong)
	{
		if (DebugMode) WriteChatf("MQ2Medley::scheduleNextSong no priority song found, returning stalest song: %s", stalestSong->name.c_str());
		return *stalestSong;
	}
	else {
		if (!quiet) WriteChatf("\arMQ2Medley\au::\atFAILED to schedule a song, no songs ready or conditions not met");
		return nullSong;
	}
}

PLUGIN_API void OnPulse()
{
	char szTemp[MAX_STRING] = { 0 };

	//DebugSpew("MQ2Medley::pulse -OnPulse()");
	if (!MQ2MedleyEnabled || !CheckCharState())
		return;

	//	if (medley.empty() && altQueue.empty()) return;
	if (medley.empty()) return;

	if (TargetSave) {
		//DebugSpew("MQ2Medley::pulse -OnPulse() in if(TargetSave)");
		//// wait for next song to start casting before switching target back
		//if (getCurrentCastingSpell() && currentSong.name.compare(szCurrentCastingSpell) == 0)
		//{
		DebugSpew("MQ2Medley::pulse - restoring target to SpawnID %d", TargetSave->SpawnID);
		pTarget = TargetSave;
		TargetSave = nullptr;
		//	return;
		//}
		//DebugSpew("MQ2Medley::pulse - state not ready to restore target");
		//return;
	}

	if (pCastingWnd && pCastingWnd->IsVisible()) {
		// Don't try to twist if the casting window is up, it implies the previous song
		// is still casting, or the user is manually casting a song between our twists
		return;
	}

	if (SongIF[0] != 0)
	{
		Evaluate(szTemp, "${If[%s,1,0]}", SongIF);
		if (DebugMode) WriteChatf("\arMQ2Medley\au::\atOnPulse SongIF[%s]=>[%s]=%d", SongIF, szTemp, atoi(szTemp));
		if (atoi(szTemp) == 0)
			return;
	}

	// get the next song
	//DebugSpew("MQ2Medley::Pulse (twist) before cast, CurrSong=%d, PrevSong = %d, CastDue -GetTime() = %d", CurrSong, PrevSong, (CastDue-GetTime()));
	if (((CastDue - GetTime()) <= 0)) {
		DebugSpew("MQ2Medley::Pulse - time for next cast");
		if (bWasInterrupted && currentSong.type != SongData::NOT_FOUND && currentSong.isReady())
		{
			bWasInterrupted = false;
			if (!quiet) WriteChatf("MQ2Medley::OnPulse Spell inturrupted - recast it");
			// current song is unchanged
		}
		else {
			if (bWasInterrupted)
			{
				if (!quiet) WriteChatf("MQ2Medley::OnPulse Spell inturrupted - spell not ready skip it");
				bWasInterrupted = false;
			}
			if (currentSong.type != SongData::NOT_FOUND)
			{
				// successful cast
				if (!currentSong.once)
					songExpires[currentSong.name] = GetTime() + (__int64)(currentSong.evalDuration() * 10);
			}
			if (!medley.empty())
			{
				currentSong = scheduleNextSong();
				if (currentSong.type == 4) return;
				if (!quiet) WriteChatf("\arMQ2Medley\au::\atScheduled: %s", currentSong.name.c_str());
				if (currentSong.targetExp.length() > 0)
					currentSong.targetID = currentSong.evalTarget();
			}
		}

		long castTime = doCast(currentSong);

		if (DebugMode) WriteChatf("MQ2Medley::OnPulse - casting time for %s - %d", currentSong.name.c_str(), castTime);
		if (castTime != -1)  // cast failed
		{
			// cast started successfully - update CastDue and PrevSong is now the song we're casting.
			CastDue = GetTime() + castTime + CAST_PAD_TIME;
		}
		else {
			DebugSpew("MQ2Medley::OnPulse - cast failed for %s", currentSong.name.c_str());
			currentSong = nullSong;
		}

		DebugSpew("MQ2Medley::OnPulse - exit handling new song: %s", currentSong.name.c_str());
	}
}

//#Event Immune "Your target cannot be mesmerized#*#"

PLUGIN_API bool OnIncomingChat(const char* Line, DWORD Color)
{
	char szMsg[MAX_STRING] = { 0 };
	if (!bTwist || !MQ2MedleyEnabled)
		return false;
	// DebugSpew("MQ2Medley::OnIncomingChat(%s)",Line);

	// if (!strcmp(Line, "You haven't recovered yet...")) WriteChatf("MQ2Medley::Have not recovered");

	if ((strstr(Line, "You miss a note, bringing your ") && strstr(Line, " to a close!")) ||
		!strcmp(Line, "You haven't recovered yet...") ||
		(strstr(Line, "Your ") && strstr(Line, " spell is interrupted."))) {
		DebugSpew("MQ2Medley::OnIncomingChat - Song Interrupt Event: %s", Line);
		bWasInterrupted = true;
		CastDue = -1;
	} else if (!strcmp(Line, "You can't cast spells while stunned!")) {
		DebugSpew("MQ2Medley::OnIncomingChat - Song Interrupt Event (stun)");
		bWasInterrupted = true;
		// Wait one second before trying again, to avoid spamming the trigger text w/ cast attempts
		CastDue = GetTime() + 10;
	}
	return false;
}

PLUGIN_API void SetGameState(int GameState)
{
	DebugSpew("MQ2Medley::SetGameState()");
	if (GameState == GAMESTATE_INGAME) {
		MQ2MedleyEnabled = true;
		PCHARINFO pCharInfo = GetCharInfo();
		if (!Initialized && pCharInfo) {
			Initialized = true;
			Load_MQ2Medley_INI(pCharInfo);
		}
	}
	else {
		if (GameState == GAMESTATE_CHARSELECT)
			Initialized = false;
		MQ2MedleyEnabled = false;
	}
}


/**
* SongData Impl
*/
SongData::SongData(std::string spellName, SpellType spellType, int spellCastTime) {
	name = spellName;
	type = spellType;
	castTime = spellCastTime;
	durationExp = "180";	// 3 min default
	targetID = 0;
	conditionalExp = "1";  // default always sing
	targetExp = "";			// expression for targetID
	once = 0;
}

SongData::~SongData() {}

bool SongData::isReady() {
	char zOutput[MAX_STRING] = { 0 };
	switch (type) {
	case SongData::SONG:
		for (int i = 0; i < NUM_SPELL_GEMS; i++)
		{
			PSPELL pSpell = GetSpellByID(GetPcProfile()->MemorizedSpells[i]);
			if (pSpell && name.compare(pSpell->Name) == 0)
				return GetSpellGemTimer(i) == 0;
		}
		return false;
	case SongData::ITEM:
		sprintf_s(zOutput, "${FindItem[=%s].Timer}", name.c_str());
		ParseMacroData(zOutput,MAX_STRING);
		DebugSpew("MQ2Medley::SongData::IsReady() ${FindItem[=%s].Timer} returned=%s", name.c_str(), zOutput);
		if (!_stricmp(zOutput, "null"))
			return false;
		return atoi(zOutput) == 0;
	case SongData::AA:
		sprintf_s(zOutput, "${Me.AltAbilityReady[%s]}", name.c_str());
		ParseMacroData(zOutput,MAX_STRING);
		DebugSpew("MQ2Medley::SongData::IsReady() ${Me.AltAbilityReady[%s]} returned=%s", name.c_str(), zOutput);
		return _stricmp(zOutput, "TRUE") == 0;
	default:
		WriteChatf("MQ2Medley::SongData::isReady - unsupported type %d for \"%s\", SKIPPING", type, name.c_str());
		return false; // todo
	}
}

double SongData::evalDuration() {
	char zOutput[MAX_STRING] = { 0 };
	sprintf_s(zOutput, "${Math.Calc[%s]}", durationExp.c_str());
	ParseMacroData(zOutput,MAX_STRING);
	if (DebugMode) WriteChatf("MQ2Medley::SongData::evalDuration() [%s] returned=%s", conditionalExp.c_str(), zOutput);

	return atof(zOutput);
}

bool SongData::evalCondition() {
	char zOutput[MAX_STRING] = { 0 };
	sprintf_s(zOutput, "${Math.Calc[%s]}", conditionalExp.c_str());
	ParseMacroData(zOutput,MAX_STRING);
	//	if (DebugMode) WriteChatf("MQ2Medley::SongData::evalCondition() [%s] returned=%s", conditionalExp.c_str(), zOutput);
	if (DebugMode) WriteChatf("MQ2Medley::SongData::evalCondition(%s) [%s] returned=%s", name.c_str(), conditionalExp.c_str(), zOutput);

	double result = atof(zOutput);
	return result != 0.0;
}

DWORD SongData::evalTarget() {
	char zOutput[MAX_STRING] = { 0 };
	sprintf_s(zOutput, "${Math.Calc[%s]}", targetExp.c_str());
	ParseMacroData(zOutput,MAX_STRING);
	if (DebugMode) WriteChatf("MQ2Medley::SongData::evalTarget(%s) [%s] returned=%s", name.c_str(), targetExp.c_str(), zOutput);

	DWORD result = atoi(zOutput);
	return result;
}
