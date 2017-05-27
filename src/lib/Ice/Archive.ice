#include "Common.ice"

/**
* Preamble (some definitions).</p>
*
* This is the Domla Archive V2.
* It operates in either of two modes, authenticated or open. The server configuration
* determines, which mode is active, the client has no way to set a mode.
* </p>
* Logged in user:
* In Authenticated mode users must login through the authentication subsystem.
* If the login succeeds, the client has to provide its session id through a
* context variable 'ID'. The parameter 'user' for the affected routines is
* not used in authenticated mode and may be empty.
* </p>
* Valid user for operation:
* If the operation is readonly either the readonly user or the
* holder of an existing lock (locker) otherwise NOT the readyonly
* user and, if the document is already locked, the holder of the lock.
* </p>
* Virtual folder structure:
* Documents are organized into a virtual folder structure which means
* the every document entry carries the information to which folder
* it belongs. Because folders do not exist in their own rights, a folder
* information gets lost if it contains no more documents.
* If an application needs to have predefined folder items and wishes to
* associate some properties with an directory, it is recommended to
* place a document named "|directory|" with the desired folder path into the archive.
* Documents carrying this name are not counted nor listed.
**/
module Access {

	/**
	* Document not found.
	**/
	exception NotFoundError extends Common::ArchiveError {
	};

	/**
	* Document is locked.
	**/
	exception LockError extends Common::ArchiveError {
	};

	/**
	* Document has been deleted.
	**/
	exception DeletedError extends Common::ArchiveError {
	};

	/**
	* Invalid arguemnts passed in.
	**/
	exception ArgumentError extends Common::ArchiveError {
	};

	/**
	* Array of binary: raw blob data.
	**/
    sequence<byte> BinaryData;
    
	/**
	* Header information for a document.
	* This struct provides header informations for a document but excludes the raw document data.
	**/
    class DocumentData extends Common::Persistable {
		/**
		* Last user who modified this entry.
		**/
        string User;
		
		/**
		* Document name, this is the file name.
		**/
        string Name;
		
		/**
		* Document display name, this is an arbitrary text for listing purposes.
		**/
        string Display;
		
		/**
		* Keywords associated for search.
		**/
        string Keywords;
		
		/** 
		* Current locker of document.
		**/
        string Locker;
		
		/**
		* Initial user.
		**/
		string Creator;
		
		/**
		* Creation timestamp.
		**/
		long Created;

		/**
		* Timestamp of last modification.
		**/
		long Modified;
		
		/**
		* Size of document.
		**/
		int Size;
		
		/**
		* Associated client defined item.
		**/
        string AssociatedItem;
		
		/**
		* Associated client defined item type.
		**/
        string AssociatedClass;
		
		/** 
		* Checksum of entry.
		**/
		string Checksum;
		
		/**
		* Folder of entry.
		**/
		string FolderPath;
		
		/** 
		* Deleted flag.
		**/
		bool Deleted;
		
		/**
		* Latest revision.
		**/
		int Revision;
    };

	/**
	* Special user name.
	* This user can only perform read-only operations
	* and never conflicts with locks.
	**/
	const string ViewOnlyUser = "\u0024\u00a7#";

	/**
	* Special document name.
	* Used to create empty directories.
	**/
	const string DocumentDirectoryName = "|directory|";
    
	/**
	* Entry has been created.
	**/
	const string Created = "created";
	
	/**
	* Filename has been changed.
	**/
	const string Renamed = "renamed";
	
	/**
	* Display name has been changed.
	**/
	const string Retitled = "retitled";
	
	/**
	* List of keywords associated with this entry has been changed.
	**/
	const string Keywords = "keywords";
	
	/**
	* New revision has been created for a document..
	**/
	const string Revision = "revision";
	
	/**
	* Document has been moved to another folder.
	**/
	const string Moved = "moved";
	
	/**
	* Document has been deleted (hidden).
	**/
	const string Deleted = "deleted";
	
	/**
	* Hidden document has been restored.
	**/
	const string Recovered = "recovered";
	
	/**
	* A link to an existing document has been created.
	**/
	const string Linked = "linked";

	/** 
	* History info entry.
	* Describes an event in the lifecycle of a document.
	**/
	class DocumentHistoryEntry extends Common::Persistable {
		/**
		* Id of document.
		**/
        string Document;

		/**
		* Revision number of this entry.
		**/
        int Revision;
		
		/**
		* Who entered this revision.
		**/
        string Actor;
		
		/**
		* Action code describing the entry.
		* More than one code per entry is possible, separator is ';'.
		**/
        string Action;
		
		/**
		* Optional description, provided by the Actor.
		**/
        string Comment;
        
		/**
		* Source of a move or link operation.
		**/
		string Source;

		/**
		* Target of a move or link operation.
		**/
        string Target;
		
		/**
		* Timestamp of operation.
		**/
        long Created;
    };

	/**
	* Raw document data.
	**/
	class DocumentContent extends Common::Persistable {
		/**
		* Id of history entry.
		**/
        string History;
		
		/**
		* Revision number of this entry.
		**/
        int Revision;
		
		/**
		* Checksum of data.
		**/
		string Checksum;
		
		/**
		* Raw document data, this may be a delta.
		**/
		BinaryData Content;
	};

	/**
	* Folder assignment.
	* Entries of this type describe the assignments of
	* documents to folders.
	**/
	class DocumentAssignment extends Common::Persistable {
		/**
		* Id of history entry.
		**/
        string History;
		
		/**
		* Revision number of this entry.
		**/
        int Revision;

		/**
		* Optional assignment information.
		**/
		string AssignmentType;

		/**
		* Optional assignment information.
		**/
		string AssignmentId;

		/**
		* Folder the referenced document is contained in.
		**/
		string Path;
	};

	/**
	* Folder info data.
	**/
	struct FolderInfo {
		/**
		* Name of folder.
		**/
		string Name;
		
		/**
		* Count of directly assigned documents (links are counted as normal documents).
		**/
		int Count;
	};

	/**
	* Storage statistics data.
	**/
	struct StorageInfo {
		/**
		* ID of storage.
		**/
		int Number;
		
		/**
		* Count of entries.
		* Hidden documents are counted as well.
		**/
		long Records;
		
		/**
		* Count of pages.
		* These are database pages.
		**/
		int Pages;
		
		/**
		* Count of unused pages.
		**/
		int Unused;
	};
	
	/**
	* Aarray of storage statistics data.
	**/
	sequence<StorageInfo> StorageInfos;

	/**
	* Archive statistics data.
	**/
	struct ArchiveInfo {
		/**
		* Total number threads used, either active or idle.
		**/
		int TotalThreads;

		/**
		* Number of threads busy with networking.
		**/
		int NetworkThreads;

		/**
		* Number of threads performing archive operations.
		**/
		int WorkerThreads;

		/**
		* Threads busy with internal stuff.
		**/
		int OtherThreads;
		/**
		* List of storage infos.
		* These are the details for every bucket in use.
		**/
		StorageInfos Details;
	};

	/** array of document info **/
	sequence<DocumentData> Documents;
	/** array of history entries **/
	sequence<DocumentHistoryEntry> DocumentHistory;
	/** array of folder infos **/
	sequence<FolderInfo> FolderInfos;
	/** array of strings **/
	sequence<string> Strings;

    /**
	* Supported archive operations.
	**/
	interface Archive {
		/**
		* Tells clients if they need to auhtenticate against the archive.
		*
		* @return true or false, depending on the server configuration.
		**/
		bool needsAuthentication();

		/**
		* Inserts or updates a document.
		* If the incoming document info already has a valid id, the operation is
		* considered an update, an insert otherwise. Do not try to provide an id
		* for an insert operation. If the document is locked (strongly recommended!)
		* the user performing the update must equal the locker.
		*
		* @param document The info for the document.
		* @param data The raw document data.
		* @param user The one who performs the operation.
		* @return The up-to-date document entry.
		**/
        DocumentData putInputData(DocumentData document, BinaryData data, string user) throws Common::ArchiveError;

		/**
		* Inserts or updates a document.
		* If the incoming document info already has a valid id, the operation is
		* considered an update, an insert otherwise. Do not try to provide an id
		* for an insert operation. If the document is locked (strongly recommended!)
		* the user performing the update must equal the locker.
		*
		* @param document The info for the document.
		* @param data The raw document data.
		* @param comment Will be added to the history entry.
		* @param user The one who performs the operation.
		* @return The up-to-date document entry.
		**/
        DocumentData putCommentedInputData(DocumentData document, BinaryData data, string comment, string user) throws Common::ArchiveError;

		/**
		* Download document content.
		* The operation will always succeed if the the document
		* exists and is not in the deleted state.
		* From the other side the following errors are possible:
		* - the document does not exist
		* - the document has been marked as deleted
		* - the document is locked and the user provided is not the locker
		*
		* @param id Unique id of document.
		* @param user Originator of operation.
		* @param offset Relative to the begin of the document data stream.
		* @param length Amount of data to fetch in a single operation (up to 249 MB is possible but not recommended, 10 MB seems to be a good choice).
		* @return The raw document data.
		**/
        DocumentContent readDocument(string id, string user, int offset, int length) throws Common::ArchiveError;

		/**
		* Download document content.
		* Same as readDocument but may be used to fetch an older revision.
		*
		* @param id Unique id of document
		* @param number Revision number to fetch, numbering from oldest to newest starts with 1
		* @param user Originator of operation
		* @param offset Relative to the begin of the document data stream
		* @param length Amount of data to fetch in a single operation (up to 249 MB is possible but not recommended, 10 MB seems to be a good choice)
		**/
        DocumentContent readDocumentRevision(string id, int number, string user, int offset, int length) throws Common::ArchiveError;
		
		/**
		* Lists all documents within a folder.
		* Fetches the infos for all documents directly placed within the given path.
		*
		* @param folderPath A slash separated path descriptor, follows the rules for unix paths.
		* @return List of document headers. Deleted documents are ignored.
		**/
		Documents listDocuments(string folderPath) throws Common::ArchiveError;

		/**
		* Lists all documents within a folder.
		* Fetches the infos for all documents directly and indirectly placed within the given path.
		* This means that documents placed in subfolders are listed as well.
		*
		* @param folderPath A slash separated path descriptor, follows the rules for unix paths.
		* @return List of document headers. Deleted documents are ignored.
		**/
		Documents listAllDocuments(string folderPath) throws Common::ArchiveError;
		
		/**
		* Lists all deleted documents within a folder.
		* Fetches the infos for all deleted documents directly and indirectly placed within the given path.
		* This means that documents placed in subfolders are listed as well.
		*
		* @param folderPath A slash separated path descriptor, follows the rules for unix paths.
		* @return List of document headers.
		**/
		Documents listDeletedDocuments(string folderPath) throws Common::ArchiveError;
		
		/**
		* Lists subfolders of a given folder.
		*
		* Reads the names of the subfolders for a given folder.
		* @param root The folder to to read the subfolder names for.
		* @return Full paths of subfolders.
		**/
		FolderInfos listFolders(string root) throws Common::ArchiveError;
		
		/**
		* Lists all known meta tags.
		* These are the names, not the values associated with the names.
		* @return Distinct list of the names all known meta tags.
		**/
		Strings listMetaTags() throws Common::ArchiveError;

		/**
		* Scans the database for matching documents.
		* Searches for documents equipped with one or more of the given keywords.
		*
		* @param keywords Space separated list of words.
		* @return List of document headers.
		**/
		Documents searchDocuments(string keywords) throws Common::ArchiveError;

		/**
		* Scans the database for matching documents.
		* Searches for documents containing one or more of the given keywords. This
		* routine uses the fulltext index, so the IFilters should be configured
		* correctly to let the fulltext search produce useful results.
		*
		* @params keywords Space separated list of words.
		* @return List of document headers.
		**/
		Documents searchDocumentsForContent(string keywords) throws Common::ArchiveError;

		/**
		* Scans the database for matching document names.
		* Searches for document names containing one or more of the given words.
		*
		* keywords: Pipe separated list of words.
		**/
		Documents searchDocumentsForFilenames(string nameParts) throws Common::ArchiveError;

		/**
		* Scans the database for matching document names.
		* Searches for document names matching the given regular expression.
		*
		* keywords: Pipe separated list of words.
		**/
		Documents searchDocumentsForExpression(string expression) throws Common::ArchiveError;

		/**
		* Scans the database for matching documents.
		* Searches for documents equipped with one or more of the given tags.
		*
		* tags: Space separated list of words.
		**/
		Documents searchDocumentsForMetas(string tags) throws Common::ArchiveError;

		/**
		* Rebuilds the fulltext index.
		* Clears and rebuilds the fulltext index. This operation may take
		* a long time and must be performed by a member of the admin group.
		**/
		void rebuildFulltext();

		/**
		* Reread the info entry for a given id.
		* The given id must refer an existing document.
		*
		* id: Unique identifier of a document.
		**/
        DocumentData refreshInfo(string id) throws Common::ArchiveError;

		/**
		* Read the complete history for a given id.
		* The given id must refer an existing document.
		*
		* id: Unique identifier of a document.
		**/
		DocumentHistory readHistory(string id) throws Common::ArchiveError;
        
		/**
		* Delete (hide) a document by id.
		* This operation is revertable, it just marks the target as hidden
		* but does not remove the document or wipes its history. The given
		* user must not be the readonly one and must held the lock if the
		* document is locked.
		*
		* id: must refer an existing, not deleted document
		* user: valid writing user
		**/
		void delete(string id, string user) throws Common::ArchiveError;
        
		/**
		* Destroys a document by id.
		* This operation is NOT revertable, it removes a document and
		* all associated informations from the database. The given
		* user must not be the readonly one and must hold the lock if
		* the document is locked.
		*
		* id: must refer an existing, not deleted document
		* user: valid writing user
		**/
		void destroy(string id, string user) throws Common::ArchiveError;

		/**
		* Moves a document to a new location.
		* The document will be moved to a new location within the
		* virtual folder structure. The source must (obviously) exist
		* at the given path, but the new path will be created if needed.
		*
		* id: identifier of document
		* oldPath: source of operation
		* newPath: target of operation
		* user: valid writing user
		**/
        void move(string id, string oldPath, string newPath, string user) throws Common::ArchiveError;

		/**
		* Copies a document to a new location.
		* The document will be copied to a new location within the
		* virtual folder structure. The source must (obviously) exist
		* at the given path, but the new path will be created if needed.
		* The new document exists independently from the old one, both
		* can be modified without inferring with each other.
		*
		* id: identifier of document
		* sourcePath: source of operation
		* targetPath: target of operation
		* user: valid writing user

		* returns: id of copied document
		**/
        string copy(string id, string sourcePath, string targetPath, string user) throws Common::ArchiveError;

		/**
		* Links a document to a new location.
		* The document will be linked to a new location within the
		* virtual folder structure. The source must (obviously) exist
		* at the given path, but the new path will be created if needed.
		* A link results in a new entry which refers to the original
		* document, which means that all modifictions are visible from
		* any link pointing to the same document.
		*
		* id: identifier of document
		* sourcePath: source of operation
		* targetPath: target of operation
		* user: valid writing user
		**/
        void link(string id, string sourcePath, string targetPath, string user) throws Common::ArchiveError;

		/**
		* Associate application defined info to a document.
		* The archive system does not care about the informations provided here.
		* Possible use cases are associate a form class to a document as the
		* viewer or bind an instance of another application item to a document.
		* The folderPath info is needed, because a document may reside at
		* multiple locations and the association is bound to a specific
		* item / folder combination.
		*
		* id: identifier of document
		* user: valid writing user
		* folderPah: 
		* associatedItem: any text
		* associatedClass: any text
		**/
        void associate(string id, string user, string folderPath, string associatedItem, string associatedClass) throws Common::ArchiveError;

		/**
		* Locks a document.
		* After a document has been locked, only the locker may unlock or
		* modify the document. The document must not be locked prior to this call.
		*
		* id: identifier of document
		* user: valid writing user
		**/
        void lockit(string id, string user) throws Common::ArchiveError;

		/**
		* Unlocks a document.
		* Only the locker may remove an existing lock.
		* The document must be locked prior to this call.
		*
		* id: identifier of document
		* user: valid writing user
		**/
        void unlock(string id, string user) throws Common::ArchiveError;

		/**
		* Undeletes a document.
		* Deleted documents are hidden, not really removed from the database.
		* This operation makes it visible again. The document must be in the
		* deleted state prior to this call.
		*
		* id: identifier of document
		* user: valid writing user
		**/
        void undelete(string id, string user) throws Common::ArchiveError;

		/**
		* Undeletes a list of documents.
		* Deleted documents are hidden, not really removed from the database.
		* This operation makes it visible again. The documents must be in the
		* deleted state prior to this call.
		*
		* ids: identifiers of documents
		* user: valid writing user
		**/
        void undeleteMultiple(Strings ids, string user) throws Common::ArchiveError;

		/**
		* Updates the display name of a document.
		* Sets a new display for a document.
		*
		* id: identifier of document
		* user: valid writing user
		* title: new display name
		**/
        void updateTitle(string id, string user, string title) throws Common::ArchiveError;

		/**
		* Updates the keywords of a document.
		* The new keywords replace the old ones.
		*
		* id: identifier of document
		* user: valid writing user
		* keywords: new display name
		**/
        void assignKeywords(string id, string user, string keywords) throws Common::ArchiveError;

		/**
		* Assigns meta tags to a document.
		*
		* id: identifier of document
		* user: valid writing user
		* tags: meta tags in the form name=value, separated by ASCII 30 (hex 1e)
		**/
        void assignMetaData(string id, string user, string tags) throws Common::ArchiveError;

		/**
		* Replaces meta tags for a document.
		*
		* id: identifier of document
		* user: valid writing user
		* tags: meta tags in the form name=value, separated by ASCII 30 (hex 1e)
		**/
        void replaceMetaData(string id, string user, string tags) throws Common::ArchiveError;

		/**
		* Replaces meta tags for a document.
		*
		* id: identifier of document
		* user: valid writing user
		* tags: meta tags in the form name=value, separated by ASCII 30 (hex 1e)
		**/
        Strings listMetaDataFor(string id) throws Common::ArchiveError;
		
		/**
		* Find a document by its file name.
		* Searches a document by checking its file name.
		*
		* folderPath: folder to search
		* name: search values
		**/
		DocumentData find(string folderPath, string name) throws Common::ArchiveError;
		
		/**
		* Load a document by its id.
		* Searches a document by checking its id.
		*
		* id: id of document
		**/
		DocumentData findById(string id) throws Common::ArchiveError;
		
		/**
		* Load a document by its id and revision number.
		* Searches a document header by checking its id and revision number.
		*
		* id: id of document
		* revision: revision number of document
		**/
		DocumentData findByIdAndRevision(string id, int revision) throws Common::ArchiveError;
		
		/**
		* Find a document by its display name.
		* Searches a document by checking its display name.
		*
		* folderPath: folder to search
		* name: search values
		**/
		Documents findTitle(string folderPath, string name) throws Common::ArchiveError;
		
		/**
		* Recalculate stored checksums.
		* The system stores a checksum for every history entry. The checksum
		* is calculated from the whole document, not only the binary delta.
		*
		* THIS IS A VERY TIME CONSUMING OPERATION!!!
		*
		**/
		void calculateHashes() throws Common::ArchiveError;

		/**
		* Reads the complete folder structure from the archive.
		**/
		FolderInfos readTree() throws Common::ArchiveError;

		/**
		* Fetches folder structure for a given branch.
		**/
		FolderInfos readBranch(string folderPath) throws Common::ArchiveError;

		/**
		* Exports all documents (included deleted ones) to
		* a given directory. This directory must be valid
		* and writable from the point of the service process.
		**/
		void exportDocuments(string targetPath) throws Common::ArchiveError;

		/**
		* Imports a bunch of documents from a given path, building the
		* folder tree from the found directory structure. This directory
		* must be valid and readable from the point of the service process.
		**/
		void importDocuments(string sourcePath, string user) throws Common::ArchiveError;

		/**
		* Performs a backup of all storage files into a single storage
		* in a given directory. This directory must be valid
		* and writable from the point of the service process. An
		* already existing backup will be removed before running
		* the actual backup. The file will be named 'domla.archive.backup'.
		**/
		void backup(string targetPath) throws Common::ArchiveError;

		/**
		* Searches for a file named 'domla.archive.backup' in the
		* given directory and rebuilds the whole archive from it.
		* This operation overwrites all exsiting archive items.
		**/
		void restore(string sourcePath, string user) throws Common::ArchiveError;

		/**
		* Fetches statistical informations from the archive.
		**/
		ArchiveInfo statistics() throws Common::ArchiveError;

		/**
		* Starts sampling statistical data.
		* This may have a performance impact.
		**/
		void startStatistics() throws Common::ArchiveError;

		/**
		* Stops sampling statistical data.
		**/
		void stopStatistics() throws Common::ArchiveError;
    };
};
