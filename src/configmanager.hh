/*
	Flexisip, a flexible SIP proxy server with media capabilities.
    Copyright (C) 2010  Belledonne Communications SARL.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef configmanager_hh
#define configmanager_hh
#if defined(HAVE_CONFIG_H) && !defined(FLEXISIP_INCLUDED)
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#include "flexisip-config.h"
#define FLEXISIP_INCLUDED
#endif
#include <string>
#include <sstream>
#include <iostream>
#include <list>
#include <cstdlib>
#include <vector>
#include <unordered_set>
#include <tuple>
#include <queue>

#include <algorithm>

#include "common.hh"
#include <typeinfo>


#ifdef ENABLE_SNMP
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>
#include <net-snmp/agent/agent_trap.h>
#else
typedef unsigned long oid;
#endif

enum class ConfigState {Check, Changed, Reset, Commited};
class ConfigValue;
class ConfigValueListener {
	static bool sDirty;
public:
	~ConfigValueListener(){};
	bool onConfigStateChanged(const ConfigValue &conf, ConfigState state);
protected:
	virtual bool doOnConfigStateChanged(const ConfigValue &conf, ConfigState state)=0;
};

enum GenericValueType{
	Boolean,
	Integer,
	Counter64,
	String,
	StringList,
	Struct,
	Notification,
	RuntimeError
};


struct ConfigItemDescriptor {
	GenericValueType type;
	const char *name;
	const char *help;
	const char *default_value;
};
static const ConfigItemDescriptor config_item_end={Boolean,NULL,NULL,NULL};

struct StatItemDescriptor {
	GenericValueType type;
	const char *name;
	const char *help;
};
static const StatItemDescriptor stat_item_end={Boolean,NULL,NULL};

class Oid {
	friend class GenericEntry;
	friend class StatCounter64;
	friend class ConfigValue;
	friend class GenericStruct;
	friend class RootConfigStruct;
	friend class NotificationEntry;
protected:
	Oid(Oid &parent, oid leaf);
	Oid(std::vector<oid> path);
	Oid(std::vector<oid> path, oid leaf);
	std::vector<oid> &getValue() {return mOidPath;}
	virtual ~Oid();
private:
	std::vector<oid> mOidPath;
public:
	std::string getValueAsString() const{
		std::ostringstream oss (std::ostringstream::out);
		for (oid i=0; i < mOidPath.size(); ++i) {
			if (i != 0) oss << ".";
			oss << mOidPath[i];
		}
		return oss.str();
	}
	oid getLeaf() { return mOidPath[mOidPath.size()-1]; }
	static oid oidFromHashedString(const std::string &str);
};


class GenericEntry;
class GenericEntriesGetter {
	static GenericEntriesGetter *sInstance;
	std::map<std::string, GenericEntry *> mEntries;
	std::unordered_set<std::string> mKeys;
public:
	static GenericEntriesGetter &get() {
		if (!sInstance) sInstance = new GenericEntriesGetter();
		return *sInstance;
	};
	void registerWithKey(const std::string &key, GenericEntry *stat) {
		if (mKeys.find(key) != mKeys.end()) {
			LOGA("Duplicate entry key %s", key.c_str());
		}
//		LOGD("Register with key %s", key.c_str());
		mEntries.insert(make_pair(key, stat));
	}
	template <typename _retType>
	_retType &find(const std::string &key)const;
};


class GenericEntry {
public:
	static std::string sanitize(const std::string &str);

	const std::string & getName()const{
		return mName;
	}
	std::string getPrettyName()const;

	GenericValueType getType()const{
		return mType;
	}
	const std::string &getHelp()const{
		return mHelp;
	}
	GenericEntry *getParent()const{
		return mParent;
	}
	virtual ~GenericEntry(){
		if (mOid) delete mOid;
	}
	virtual void setParent(GenericEntry *parent);
	/*
	 * @returns entry oid built from parent & object oid index
	 */
	Oid& getOid() {return *mOid;};
	std::string getOidAsString() const {
		return mOid->getValueAsString();
	}
	void setErrorMessage(const std::string &msg) {mErrorMessage=msg;}
	std::string &getErrorMessage() {
		return mErrorMessage;
	}

	void setReadOnly(bool ro) {mReadOnly=ro;};
	bool getExportToConfigFile() {return mExportToConfigFile;}
	void setExportToConfigFile(bool val) { mExportToConfigFile=val;}
#ifdef ENABLE_SNMP
	static int sHandleSnmpRequest(netsnmp_mib_handler *handler,
			netsnmp_handler_registration *reginfo,
			netsnmp_agent_request_info   *reqinfo,
			netsnmp_request_info         *requests);
	virtual int handleSnmpRequest(netsnmp_mib_handler *,
			netsnmp_handler_registration *,netsnmp_agent_request_info*,netsnmp_request_info*){return -1;};
#endif
	virtual void mibFragment(std::ostream &ostr, std::string spacing)const=0;
	void registerWithKey(const std::string &key) {
		GenericEntriesGetter::get().registerWithKey(key, this);
	}
	void setConfigListener(ConfigValueListener *listener) {
		mConfigListener=listener;
	}
	ConfigValueListener *getConfigListener() const {
		return mConfigListener;
	}
protected:
	virtual void doMibFragment(std::ostream &ostr, const std::string &def, const std::string &access, const std::string &syntax, const std::string &spacing) const;
	GenericEntry(const std::string &name, GenericValueType type, const std::string &help,oid oid_index=0);
	Oid *mOid;
	const std::string mName;
	bool mReadOnly;
	bool mExportToConfigFile;
	std::string mErrorMessage;
private:
	const std::string mHelp;
	GenericValueType mType;
	GenericEntry *mParent;
	ConfigValueListener *mConfigListener;
	oid mOidLeaf;
};


template <typename _retType>
_retType &GenericEntriesGetter::find(const std::string &key)const{
	auto it=mEntries.find(key);
	if (it == mEntries.end()) {
		LOGA("Entry not found %s", key.c_str());
	}

	GenericEntry *ge=(*it).second;
	_retType *ret=dynamic_cast<_retType *>(ge);
	if (!ret) {
		LOGA("%s - bad entry type %s", key.c_str(), typeid(*ge).name());
	}

	return *ret;
};



class ConfigValue;
class StatCounter64;
class GenericStruct : public GenericEntry{
public:
	GenericStruct(const std::string &name, const std::string &help,oid oid_index);
	GenericEntry * addChild(GenericEntry *c);
	StatCounter64 *createStat(const std::string &name, const std::string &help);
	std::pair<StatCounter64 *, StatCounter64 *> createStatPair(const std::string &name, const std::string &help);
	void addChildrenValues(ConfigItemDescriptor *items);
	void addChildrenValues(ConfigItemDescriptor *items, bool hashed);
	//void addChildrenValues(StatItemDescriptor *items);
	std::list<GenericEntry*> &getChildren();
	template <typename _retType>
	_retType *get(const char *name)const;
	template <typename _retType>
	_retType *getDeep(const char *name, bool strict)const;
	~GenericStruct();
	GenericEntry *find(const char *name)const;
	GenericEntry *findApproximate(const char *name)const;
	virtual void mibFragment(std::ostream & ost, std::string spacing) const;
	virtual void setParent(GenericEntry *parent);
private:
	std::list<GenericEntry*> mEntries;
};

class RootConfigStruct : public GenericStruct {
public:
	RootConfigStruct(const std::string &name, const std::string &help, std::vector<oid> oid_root_prefix);
};


class StatCounter64 : public GenericEntry{
public:
	StatCounter64(const std::string &name, const std::string &help, oid oid_index);
#ifdef ENABLE_SNMP
	virtual int handleSnmpRequest(netsnmp_mib_handler *,
			netsnmp_handler_registration *,netsnmp_agent_request_info*,netsnmp_request_info*);
#endif
	virtual void mibFragment(std::ostream & ost, std::string spacing) const;
	void setParent(GenericEntry *parent);
	uint64_t read() { return mValue; }
	void set(uint64_t val) { mValue=val; }
	void operator++() {++mValue;};
	void operator++(int count) {mValue+=count;};
	void operator--() {--mValue;};
	void operator--(int count) {mValue-=count;};
private:
	uint64_t mValue;
};



class StatFinishListener {
	std::unordered_set<StatCounter64*> mStatList;
public:
	void addStatCounter(StatCounter64 *stat) {
		mStatList.insert(stat);
	}
	~StatFinishListener(){
		for(auto it=mStatList.begin(); it != mStatList.end(); ++it) {
			StatCounter64 &s = **it;
			++s;
		}
	}
};

class ConfigValue : public GenericEntry{
public:
	ConfigValue(const std::string &name, GenericValueType  vt, const std::string &help, const std::string &default_value,oid oid_index);
	void set(const std::string &value);
	void setNextValue(const std::string &value);
	virtual const std::string &get()const;
	const std::string &getNextValue()const { return mNextValue; }
	const std::string &getDefault()const;
	void setDefault(const std::string &value);
	void setNotifPayload(bool b) { mNotifPayload=b;}
#ifdef ENABLE_SNMP
	virtual int handleSnmpRequest(netsnmp_mib_handler *,
			netsnmp_handler_registration *,netsnmp_agent_request_info*,netsnmp_request_info*);
#endif
	virtual void setParent(GenericEntry *parent);
	virtual void mibFragment(std::ostream & ost, std::string spacing) const;
	virtual void doMibFragment(std::ostream &ostr, const std::string &syntax, const std::string &spacing) const;
protected:
	bool invokeConfigStateChanged(ConfigState state) {
		if (getParent() && getParent()->getType() == Struct) {
			ConfigValueListener *listener = getParent()->getConfigListener();
//			LOGD("invokeConfigStateChanged to %d on %s/%s", state,
//					getParent()->getName().c_str(), mName.c_str());
			if (listener) {
				return listener->onConfigStateChanged(*this, state);
			} else {
				LOGE("%s doesn't implement a config change listener.",
						getParent()->getName().c_str());
			}
		}
		return true;
	}
	std::string mNextValue;
private:
	std::string mValue;
	std::string mDefaultValue;
	bool mNotifPayload;
};

class ConfigBoolean : public ConfigValue{
public:
	ConfigBoolean(const std::string &name, const std::string &help, const std::string &default_value,oid oid_index);
	bool read()const;
	bool readNext()const;
	void write(bool value);
#ifdef ENABLE_SNMP
	virtual int handleSnmpRequest(netsnmp_mib_handler *,
			netsnmp_handler_registration *,netsnmp_agent_request_info*,netsnmp_request_info*);
#endif
	virtual void mibFragment(std::ostream & ost, std::string spacing)const;
};

class ConfigInt : public ConfigValue{
public:
#ifdef ENABLE_SNMP
	virtual int handleSnmpRequest(netsnmp_mib_handler *,
			netsnmp_handler_registration *,netsnmp_agent_request_info*,netsnmp_request_info*);
#endif
	ConfigInt(const std::string &name, const std::string &help, const std::string &default_value,oid oid_index);
	virtual void mibFragment(std::ostream & ost, std::string spacing) const;
	int read()const;
	int readNext()const;
	void write(int value);
};


class ConfigRuntimeError : public ConfigValue {
	mutable std::string mErrorStr;
public:
	ConfigRuntimeError(const std::string &name, const std::string &help, oid oid_index);
	std::string generateErrors()const;
#ifdef ENABLE_SNMP
	virtual int handleSnmpRequest(netsnmp_mib_handler *,
			netsnmp_handler_registration *,netsnmp_agent_request_info*,netsnmp_request_info*);
#endif
//	std::string &read()const { return get(); }
	const void writeErrors(GenericEntry *entry, std::ostringstream &oss) const;
};

class ConfigString : public ConfigValue{
public:
	ConfigString(const std::string &name, const std::string &help, const std::string &default_value,oid oid_index);
	const std::string & read()const;
};

class ConfigStringList : public ConfigValue{
public:
	ConfigStringList(const std::string &name, const std::string &help, const std::string &default_value,oid oid_index);
	std::list<std::string> read()const;
private:
};

template <typename _retType>
_retType *GenericStruct::get(const char *name)const{
	GenericEntry *e=find(name);
	if (e==NULL) {
		LOGA("No ConfigEntry with name %s in struct %s",name,getName().c_str());
		return NULL;
	}
	_retType *ret=dynamic_cast<_retType *>(e);
	if (ret==NULL){
		LOGA("Config entry %s in struct %s does not have the expected type",name,e->getParent()->getName().c_str());
		return NULL;
	}
	return ret;
};


template <typename _retType>
_retType *GenericStruct::getDeep(const char *name, bool strict)const{
	if (!name) return NULL;
	std::string sname(name);

	size_t len=sname.length();
	size_t next,prev=0;
	const GenericStruct *next_node,*prev_node=this;
	while (std::string::npos != (next=sname.find('/', prev))) {
		std::string next_node_name=sname.substr(prev, next-prev);
//		LOGE("Searching for node %s", next_node_name.c_str());
		GenericEntry *e=find(next_node_name.c_str());
		if (!e) {
			if (!strict) return NULL;
			LOGE("No ConfigEntry with name %s in struct %s",name,prev_node->getName().c_str());
			for (auto it=prev_node->mEntries.begin(); it != prev_node->mEntries.end(); ++it) {
				LOGE("-> %s", (*it)->getName().c_str());
			}
			LOGF("end");
			return NULL;
		}
		next_node=dynamic_cast<GenericStruct *>(e);
		if (!next_node){
			LOGA("Config entry %s in struct %s does not have the expected type",name,e->getParent()->getName().c_str());
			return NULL;
		}
		prev_node=next_node;
		prev=next+1;
	}

	std::string leaf(sname.substr(prev, len-prev));
//	LOGE("Searching for leaf %s", leaf.c_str());
	return prev_node->get<_retType>(leaf.c_str());
};

class FileConfigReader{
public:
	FileConfigReader(GenericStruct *root) : mRoot(root),mCfg(NULL),mHaveUnreads(false){
	}
	int read(const char *filename);
	int reload();
	void checkUnread();
	~FileConfigReader();
private:
	int read2(GenericEntry *entry, int level);
	static void onUnreadItem(void *p, const char *secname, const char *key, int lineno);
	void onUnreadItem(const char *secname, const char *key, int lineno);
	GenericStruct *mRoot;
	struct _LpConfig *mCfg;
	bool mHaveUnreads;
};

class NotificationEntry : public GenericEntry{
	Oid &getStringOid();
	bool mInitialized;
	std::queue<std::tuple<const GenericEntry *,std::string>> mPendingTraps;
public:
	NotificationEntry(const std::string &name, const std::string &help, oid oid_index);
	virtual void mibFragment(std::ostream & ost, std::string spacing) const;
	void send(const std::string &msg);
	void send(const GenericEntry *source, const std::string &msg);
	void setInitialized(bool status);
};

class GenericManager : protected ConfigValueListener {
	friend class ConfigArea;
public:
	static GenericManager *get();
	void declareArea(const char *area_name, const char *help, ConfigItemDescriptor *items);
	int load(const char* configFile);
	GenericStruct *getRoot();
	std::string &getConfigFile() { return mConfigFile; }
	void setOverrideMap(const std::map<std::string,std::string> overrides) { mOverrides=overrides;}
	std::map<std::string,std::string> &getOverrideMap() { return mOverrides; }
	const GenericStruct *getGlobal();
	void loadStrict();
	StatCounter64 &findStat(const std::string &key);
	void addStat(const std::string &key, StatCounter64 &stat);
	NotificationEntry *getSnmpNotifier() { return mNotifier; }
	void sendTrap(const GenericEntry *source, const std::string &msg) {
		mNotifier->send(source, msg);
	}
	void sendTrap(const std::string &msg) {
		mNotifier->send(&mConfigRoot,msg);
	}
	bool mNeedRestart;
	bool mDirtyConfig;
private:
	GenericManager();
	virtual ~GenericManager(){}
	bool doIsValidNextConfig(const ConfigValue &cv);
	bool doOnConfigStateChanged(const ConfigValue &conf, ConfigState state);
	void applyOverrides(GenericStruct *root, bool strict) {
		for (auto it=mOverrides.begin(); it != mOverrides.end(); ++it){
			const std::string &key((*it).first);
			const std::string &value((*it).second);
			if (value.empty()) continue;
			ConfigValue *val=root->getDeep<ConfigValue>(key.c_str(), strict);
			if (val) {
				std::cout << "Overriding config with " << key << ":" << value << std::endl;
				val->set(value);
			} else {
				std::cout << "Skipping config override " << key << ":" << value << std::endl;
			}
		}
	}
	static void atexit(); // Don't call directly!
	RootConfigStruct mConfigRoot;
	FileConfigReader mReader;
	std::string mConfigFile;
	std::map<std::string,std::string> mOverrides;
	static GenericManager *sInstance;
	std::map<std::string,StatCounter64*> mStatMap;
	std::unordered_set<std::string> mStatOids;
	NotificationEntry *mNotifier;
};


class FileConfigDumper{
public:
	FileConfigDumper(GenericStruct *root){
		mRoot=root;
		mDumpDefault=true;
	}
	std::ostream &dump(std::ostream & ostr)const;
	void setDumpDefaultValues(bool value) {mDumpDefault=value;}
private:
	bool mDumpDefault;
	std::ostream & printHelp(std::ostream &os, const std::string &help, const std::string &comment_prefix)const;
	std::ostream &dump2(std::ostream & ostr, GenericEntry *entry, int level)const;
	GenericStruct *mRoot;
};

inline std::ostream & operator<<(std::ostream &ostr, const FileConfigDumper &dumper){
	return dumper.dump(ostr);
}

class TexFileConfigDumper{
public:
	TexFileConfigDumper(GenericStruct *root){
		mRoot=root;
	}
	std::ostream &dump(std::ostream & ostr)const;
private:
	std::string formatTitle(const std::string &strc) const;

	std::string escape(const std::string &strc) const;

	std::ostream &dump2(std::ostream & ostr, GenericEntry *entry, int level)const;
	GenericStruct *mRoot;
};

inline std::ostream & operator<<(std::ostream &ostr, const TexFileConfigDumper &dumper){
	return dumper.dump(ostr);
}

class MibDumper{
public:
	MibDumper(GenericStruct *root){
		mRoot=root;
	}
	std::ostream &dump(std::ostream & ostr)const;
private:
	std::ostream &dump2(std::ostream & ostr, GenericEntry *entry, int level)const;
	GenericStruct *mRoot;
};

inline std::ostream & operator<<(std::ostream &ostr, const MibDumper &dumper){
	return dumper.dump(ostr);
}



#endif
