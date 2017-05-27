#include "Common.ice"

module Authentication {
	class User;
	class Group;
	class Right;

	enum Permissions {
		None = 0,
		Denied = 1,
		ReadOnly = 2,
		ReadWrite = 6,
		Delete = 14,
		All = 65534
	};

	struct NamedItem {
		string Name;
		string DisplayName;
	};

	struct FolderPermission {
		string Folder;
		Permissions Permission;
	};

	// Authentication specific exceptions
	exception AuthenticationError extends Common::ArchiveError {
	};

	["clr:generic:List"]sequence<string> Strings;
	["clr:generic:List"]sequence<User> UserSequence;
	["clr:generic:List"]sequence<Group> GroupSequence;
	["clr:generic:List"]sequence<Right> RightSequence;
	["clr:generic:List"]sequence<NamedItem> NameSequence;
	["clr:generic:List"]sequence<FolderPermission> FolderPermissions;

	class Right extends Common::Persistable {
		string OwnerId;
		string Folder;
		Permissions Permission;
	};
		
	class User extends Common::Persistable {
        string Name;
        string DisplayName;
        string Password;
		bool IsAdmin;
		RightSequence Rights;
		GroupSequence Groups;
	};

	class Group extends Common::Persistable {
        string Name;
        string DisplayName;
		bool IsAdmin;
		RightSequence Rights;
		UserSequence Users;
	};

	/**
	 Access to the authentication subsystem.
	**/
	interface Sentinel {
		/**
		 Logon to the archive system.
		 
		 @param name The name of the user who requests access.
		 @param password Password of the user.
		 
		 @return Either a session id (logon succeeded) or empty string (logon denied)
		**/
		string Logon(string name, string password) throws AuthenticationError;
		
		/**
		 Logoff from the archive system.
		 
		 @param key Valid session id.
		**/
		void Logoff(string key) throws AuthenticationError;

		/**
		 Send an heartbeat.</p>

		 Sessions are kept alive due to interaction otherwise they
		 timeout after 120 sec. This allows an idle client app to
		 tell the server that the client is not gone.
		 
		 @param key Valid session id.
		**/
		void KeepAlive(string key) throws AuthenticationError;
		
		/**
		 Create a new user.</p>

		 Users may only be created by su or by members of the
		 'administrators' group.
		 
		 @param key Valid session id of an admin user.
		 @param name Login of the new user, converted to lowercase.
		 @param display Full name of the new user, not modified in any way.
		 @param password password for the new user
		 @param initialGroup first group the new user is added to
		**/
		void CreateUser(string key, string name, string display, string password, string initialGroup) throws AuthenticationError;

		/**
		 Delete a user.</p>

		 Users may only be deleted by su or by members of the
		 'administrators' group. It is not possible to drop 'su'.
		 
		 @param key Valid session id of an admin user.
		 @param name Name of the user to drop.
		**/
		void DropUser(string key, string name) throws AuthenticationError;

		/**
		 Create a new group.</p>

		 Groups may only be created by su or by members of the
		 'administrators' group.
		 
		 @param key Valid session id of an admin user.
		 @param name Name of the new group, converted to lowercase.
		 @param display Description or long name of the new group, not modified in any way.
		**/
		void CreateGroup(string key, string name, string display) throws AuthenticationError;

		/**
		 Delete a group.</p>

		 Groups may only be deleted by su or by members of the
		 'administrators' group. It is not possible to drop the
		 'administrators' group.
		 
		 @param key Valid session id of an admin user.
		 @param name Name of the group to drop.
		**/
		void DropGroup(string key, string name) throws AuthenticationError;

		/**
		 Read the user permissions for a specific folder.
		 
		 @param key Valid session id of an admin user or the logged in user represented by 'name'.
		 @param name Name of the requested user.
		 @param folder Folder to calculate the permissions for.
		 @return The permissions.
		**/
		Permissions ReadFor(string key, string name, string folder) throws AuthenticationError;

		/**
		 Set a new password for a user.</p>
		 
		 Passwords can only changed by users for themselfes.

		 @param key Valid session id.
		 @param newPassword New password for the current login.
		**/
		void ChangePassword(string key, string newPassword) throws AuthenticationError;

		/**
		 List the login names of all known users.
		 
		 @param key Valid session id of an admin user.
		 @return List of login names.
		**/
		NameSequence UserNames(string key) throws AuthenticationError;

		/**
		 List the names of all known groups.
		 
		 @param key Valid session id of an admin user.
		 @return List of group names.
		**/
		NameSequence GroupNames(string key) throws AuthenticationError;

		/**
		 List the login names in a specific group.
		 
		 @param key Valid session id of an admin user.
		 @param name Name of a group.
		 @return List of login names.
		**/
		Strings UsersInGroup(string key, string name) throws AuthenticationError;

		/**
		 List the group names a user is a member of.
		 
		 @param key Valid session id of an admin user.
		 @param name Name of a user.
		 @return List of group names.
		**/
		Strings GroupsOfUser(string key, string name) throws AuthenticationError;

		/**
		 Set the access rights for a user / folder combination.</p>
		 
		 Access rights are inherited from the ancestors of a folder.
		 Specific rights have precedence of inherited rights, user
		 rights have presedence over group rights.

		 @param key Valid session id of an admin user.
		 @param name Name of a user.
		 @param folder Name of a folder.
		 @param what Permission to set, None means removal of a specific entry.
		**/
		void SetUserRights(string key, string name, string folder, Permissions what) throws AuthenticationError;

		/**
		 Set the access rights for a group / folder combination.</p>
		 
		 Access rights are inherited from the ancestors of a folder.
		 Specific rights have precedence of inherited rights, user
		 rights have presedence over group rights.

		 @param key Valid session id of an admin user.
		 @param name Name of a group.
		 @param folder Name of a folder.
		 @param what Permission to set, None means removal of a specific entry.
		**/
		void SetGroupRights(string key, string name, string folder, Permissions what) throws AuthenticationError;

		/**
		 List all specific rights for a user.
		 
		 @param key Valid session id of an admin user.
		 @param name Name of a user.
		 @return List of folder / permission pairs.
		**/
		FolderPermissions UserFolderRights(string key, string name) throws AuthenticationError;

		/**
		 List all specific rights for a group.
		 
		 @param key Valid session id of an admin user.
		 @param name Name of a group.
		 @return List of folder / permission pairs.
		**/
		FolderPermissions GroupFolderRights(string key, string name) throws AuthenticationError;

		/**
		 Add users to an existing group.
		 
		 @param key Valid session id of an admin user.
		 @param target Name of a group.
		 @param users List of login names.
		**/
		void AddUsersToGroup(string key, string target, Strings users) throws AuthenticationError;

		/**
		 Remove users from a group.
		 
		 @param key Valid session id of an admin user.
		 @param target Name of a group.
		 @param users List of login names.
		**/
		void RemoveUsersFromGroup(string key, string target, Strings users) throws AuthenticationError;

		/**
		 List all known users.
		 
		 @param key Valid session id of an admin user.
		 @return List of user instances.
		**/
		UserSequence Users(string key) throws AuthenticationError;

		/**
		 List all known groups.
		 
		 @param key Valid session id of an admin user.
		 @return List of group instances.
		**/
		GroupSequence Groups(string key) throws AuthenticationError;
	};
};
