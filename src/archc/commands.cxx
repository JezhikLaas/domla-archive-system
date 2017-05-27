#include <memory>
#include <iostream>
#include <tuple>
#include <numeric>
#include <exception>
#include <string>
#include <boost/format.hpp>
#include <Ice/Ice.h>
#include "commands.hxx"
#include "command.hxx"
#include "backup.hxx"
#include "args.hxx"
#include "lib/Archive.h"
#include "lib/Authentication.h"

using namespace std;
using boost::format;
using boost::io::group;

struct ArchiveAccess
{
    string Address;
    int Port;
    string Login;
    string Password;
    string Program;
    vector<string>::const_iterator Start;
    vector<string>::const_iterator End;
};

void archiveAction(const ArchiveAccess& access, shared_ptr<CommandBase> action)
{
    CommandArguments arguments {
        access.Program,
        access.Start,
        access.End
    };
    
    action->evaluate(arguments);
    if (action->error().empty() == false) {
        cerr << "Argument error: " << action->error() << endl;
        return;
    }
    
    auto iceHandle = Ice::initialize();
    if (!iceHandle)  throw runtime_error("ICE initialize failed");
    try {
        auto connection = (format("archivedocuments:default -h %1% -p %2%") % access.Address % access.Port).str();
        auto archiveProxy = iceHandle->stringToProxy(connection.c_str());
        auto archive = Access::ArchivePrx::checkedCast(archiveProxy);
        
        if (!archive) throw runtime_error("invalid archive proxy");
        
        Ice::Context context;
        FinishedType finish = nullptr;
        
        if (archive->needsAuthentication()) {
            cout << "archive requires authentication, logging on" << endl;
            auto authenticationProxy = iceHandle->stringToProxy((format("archiveauthentication:default -h %1% -p %2%") % access.Address % access.Port).str());
            auto authentication = Authentication::SentinelPrx::checkedCast(authenticationProxy);

            if (!authentication) throw runtime_error("invalid authentication proxy");
        
            auto token = authentication->Logon(access.Login, access.Password);
            if (token.empty()) throw runtime_error("logon failed");
            context["ID"] = token;
            
            finish = [authentication, token]() {
                cout << "done, logging off" << endl;
                authentication->Logoff(token);
            };
        }
        
        ExecutionArguments execution {
            archive,
            context,
            finish
        };
        
        action->execute(execution);
        if (iceHandle) iceHandle->destroy();
    }
    catch(...) {
        if (iceHandle) iceHandle->destroy();
        throw;
    }
}

void dispatch(int count, char** arguments)
{
    unordered_map<string, shared_ptr<CommandBase>> map{
        {"backup", shared_ptr<CommandBase>(new Backup())},
        {"rebuild", shared_ptr<CommandBase>(new Rebuild())},
        {"restore", shared_ptr<CommandBase>(new Restore())},
    };

    const vector<string> args(arguments + 1, arguments + count);
    int position = 1;
    
    auto description = accumulate(
        next(map.begin()),
        map.end(),
        string("Valid commands are ") + map.begin()->first,
        [&map, &position](string c, pair<string, shared_ptr<CommandBase>> e) {
            return c + (map.size() == ++position ? " and " : ", ") + e.first;
        }
    );
    
    args::ArgumentParser parser("This is the ARCHive Commander", description);
    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});
    
    parser.Prog(arguments[0]);
    parser.ProglinePostfix("{command options}");
    
    args::Flag version(parser, "version", "Show the version of this program", {"version"});
    args::ValueFlag<string> address(parser, "address (localhost)", "Specify the archive host or ip", {'a', "address"});
    args::ValueFlag<int> port(parser, "port number (1982)", "Specify the archive port", {'n', "port"});
    args::ValueFlag<string> user(parser, "username (su)", "Login to the archive", {'u', "user"});
    args::ValueFlag<string> password(parser, "password (<empty>)", "Login password", {'p', "password"});
    
    args::MapPositional<string, shared_ptr<CommandBase>> command(parser, "command", "Command to execute", map);
    command.KickOut(true);
    
    try {
        auto next = parser.ParseArgs(args);
        ArchiveAccess Access {
            bool{address} ? args::get(address) : "localhost",
            bool{port} ? args::get(port) : 2100,
            bool{user} ? args::get(user) : "su",
            bool{password} ? args::get(password) : "",
            arguments[0],
            next,
            end(args)
        };
        
        if (command) {
            archiveAction(Access, args::get(command));
        }
        else {
            cout << parser;
        }
    }
    catch (args::Help)
    {
        cout << parser;
    }
    catch (args::Error e)
    {
        cerr << e.what() << endl;
        cerr << parser;
    }
}
