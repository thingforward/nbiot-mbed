#include "controls.h"
#include "modemresponse.h"

namespace Narrowband {

ControlBase::ControlBase(const ControlBase& rhs) : 
    _cab(rhs._cab), _readable(rhs._readable), _writeable(rhs._writeable),
    _read_timeout(rhs._read_timeout), _write_timeout(rhs._write_timeout) {
}

bool ControlBase::d( const string & cmd, unsigned int timeout) const {
    ModemResponse r;
    if (_cab.send(cmd.c_str(), r, timeout)) {
        return r.isOk();
    }
    return false;
}

// executes cmd with a default timeout (TODO),
// takes the first non-echo line from result responses as
// return. returns empty string in case of error or
// empty responses.
string ControlBase::e( const string & cmd, unsigned int timeout) const {
    ModemResponse r;
    if (_cab.send(cmd.c_str(), r, timeout)) {
        if ( r.isOk()) {
            if ( r.getResponses().size() > 0) {
                string s = r.getResponses().front();
                // check for echo enabled, remove echo
                if ( s == cmd) {
                    r.getResponses().pop_front();
                }
                s = r.getResponses().front();
                return s;
            }
        };
    }
    return "";
}

string ControlBase::f( const string & cmd, const string & key, unsigned int timeout) const {
    ModemResponse r;
    string res = "";
    if (_cab.send(cmd.c_str(), r, timeout)) {
        if ( r.isOk()) {
            (void)r.getCommandResponse(key, res);
        };
    }
    return res;
}

StringControl::StringControl(CommandAdapterBase& cab, string cmdread, string cmdwrite, bool readable_, bool writeable_) :
    ControlBase(cab,readable_,writeable_), _cmdread(cmdread), _cmdwrite(cmdwrite) {
}

StringControl::StringControl(const StringControl& rhs) :
    ControlBase(rhs._cab, rhs._readable, rhs._writeable), _cmdread(rhs._cmdread), _cmdwrite(rhs._cmdwrite) {

}

string StringControl::get() const {
    string s;
    get(s);
    return s;
}

bool StringControl::get(string &value) const {
    if ( readable()) {
        ModemResponse r;
        if (_cab.send(_cmdread.c_str(), r, _read_timeout)) {
            if ( r.isOk()) {
                if ( r.getResponses().size() > 0) {
                    value = r.getResponses().front();
                    // check for echo enabled, remove echo
                    if ( value == _cmdread) {
                        r.getResponses().pop_front();
                    }
                    value = r.getResponses().front();
                    return true;
                }
            };
        }

    }
    return false;
}

bool StringControl::set(string value) const {
    if ( writeable()) {
        ModemResponse r;
        string lcmd = _cmdwrite + value;
        if (_cab.send(lcmd.c_str(), r, _write_timeout)) {
            return r.isOk();
        }
    }
    return false;
}


OnOffControl::OnOffControl(CommandAdapterBase& cab, string cmd, string key, bool readable_, bool writeable_) :
    ControlBase(cab,readable_,writeable_), _cmdread(cmd+"?"), _cmdwrite(cmd+"="), _key(key) {
}

OnOffControl::OnOffControl(CommandAdapterBase& cab, string cmdread, string cmdwrite, string key, bool readable_, bool writeable_) :
    ControlBase(cab,readable_,writeable_), _cmdread(cmdread), _cmdwrite(cmdwrite), _key(key) {
}

OnOffControl::OnOffControl(const OnOffControl& rhs) :
    ControlBase(rhs._cab, rhs._readable, rhs._writeable), _cmdread(rhs._cmdread), _cmdwrite(rhs._cmdwrite), _key(rhs._key) {

}

bool OnOffControl::get() const {
    bool s;
    get(s);
    return s;
}

bool OnOffControl::get(bool &value) const {
    if ( readable()) {
        ModemResponse r;
        if (_cab.send(_cmdread.c_str(), r, _read_timeout)) {
            if ( r.isOk()) {
                // keyed?
                if (_key.length() > 0) {
                    string v;
                    if (r.getCommandResponse(_key,v)) {
                        value = ( v == "1");
                        return true;
                    }
                } else {
                    // indexed, take first response.
                    if ( r.getResponses().size() > 0) {
                        string v = r.getResponses().front();
                        // check for echo enabled, remove echo
                        if ( v == _cmdread) {
                            r.getResponses().pop_front();
                        }
                        v = r.getResponses().front();
                        value = ( v == "1");
                        return true;
                    }
                }
            };
        }

    }
    return false;
}

bool OnOffControl::set(bool value) const {
    if ( writeable()) {
        ModemResponse r;
        string lcmd = _cmdwrite + (value?"1":"0");
        if (_cab.send(lcmd.c_str(), r, _write_timeout)) {
            return r.isOk();
        }
    }
    return false;
}



OperatorSelectionControl::OperatorSelectionControl(CommandAdapterBase& cab) :
    ControlBase(cab) {

}

OperatorSelectionControl::OperatorSelectionControl(const OperatorSelectionControl& rhs) :
    ControlBase(rhs), _mode(rhs._mode), _operatorName(rhs._operatorName) {

}

bool OperatorSelectionControl::get() {
    // AT+COPS?
    return false;
}

bool OperatorSelectionControl::set() {
    // AT+COPS=
    return false;
}



PDPContextControl::PDPContextControl(CommandAdapterBase& cab) :
    ControlBase(cab) {

}

PDPContextControl::PDPContextControl(const PDPContextControl& rhs) :
    ControlBase(rhs), _contexts (rhs._contexts) {

}

bool PDPContextControl::query() {
    ModemResponse r;
    if (_cab.send("AT+CGDCONT?", r, _read_timeout)) {
        if ( r.isOk()) {
            // parse contexts
            _contexts.clear();

            multimap<string,string>& m = r.getCommandResponses();
            for ( multimap<string,string>::iterator it = m.begin(); it != m.end(); ++it) {
                string line = (*it).second;
                // parse line
                string buf;
                int idx = 0;

                PDPContext c;

                for ( string::iterator it = line.begin(); it != line.end(); ++it) {
                    char n = (*it);
                    if ( n != ',') {
                        buf += n;
                    } else {
                        if ( idx == 0) {
                            c.cid = atoi(buf.c_str());
                        }
                        if ( idx == 1) {
                            c.type = buf;
                        }
                        if ( idx == 2) {
                            c.apn = buf;
                        }

                        idx++;
                        buf = "";
                    }
                }

                _contexts.insert(pair<int,PDPContext>(c.cid, c));
            }
    
            return true;
        }
    }

    return false;
}

bool PDPContextControl::hasContextByTypeAndAPN(string type, string apn) {
    for ( Narrowband::PDPContextList::const_iterator it = _contexts.begin(); it != _contexts.end(); ++it) {
        Narrowband::PDPContext d = (*it).second;
        if (d.type == type && d.apn == apn) {
            return true;
        }
    }
    return false;
}

bool PDPContextControl::set(const PDPContext& c) {
    char buf[128];
    memset(buf,0,sizeof(buf));

    snprintf(buf,sizeof(buf),"AT+CGDCONT=%d,\"%s\",\"%s\"", 
        c.cid,c.type.c_str(),c.apn.c_str());
    ModemResponse r;
    if (_cab.send(buf, r, _write_timeout)) {
        return r.isOk();
    }

    return false;
}

BandControl::BandControl(CommandAdapterBase& cab) : ControlBase(cab) { }

BandControl::BandControl(const BandControl& rhs) : ControlBase(rhs) { }

list<int> BandControl::supportedBands() const {
    std::string v = f("AT+NBAND=?", "+NBAND", _read_timeout);
    return csv_to_intlist(v);
}

list<int> BandControl::activeBands() const {
    std::string v = f("AT+NBAND?", "+NBAND", _read_timeout);
    return csv_to_intlist(v);
}

bool BandControl::set(const list<int>& b) const {
    string l = "AT+NBAND=";
    for ( list<int>::const_iterator it = b.begin(); it != b.end(); ++it) {
        char buf[16];
        snprintf(buf,sizeof(buf),"%d,",(int)(*it));
        l.append(buf);
    }
    l = l.erase(l.length()-1,1);

    ModemResponse r;
    if( _cab.send(l.c_str(), r, _write_timeout)) {
        return r.isOk();
    }
    return false;
}

list<int> BandControl::csv_to_intlist(string line) const {
    string buf;
    int idx = 0;

    list<int> res;
    for ( string::iterator it = line.begin(); it != line.end(); ++it) {
        char n = (*it);
        if ( n != ',') {
            buf += n;
        } else {
            res.push_back(atoi(buf.c_str()));
            idx++;
            buf = "";
        }
    }

    return res;
}

}