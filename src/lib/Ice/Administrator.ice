#include "Common.ice"

/**
* Administrative tasks.
**/
module Administrator {
	/**
	* Single user mode operations.
	* These routines would conflict with the normal archive operations,
	* so they must be performed by an administrator while the other
	* modules are paused.
	**/
	interface Commander {
		/**
		* Export documents to a folder structure.
		* Exports all documents (included deleted ones) to
		* a given directory. This directory must be valid
		* and writable from the point of the service process.
		* The caller must be a valid admin user, the service does not have to but can be paused.
		*
		* @param targetPath Absolute or relative path as seen by the service process.
		**/
		void exportDocuments(string targetPath) throws Common::ArchiveError;

		/**
		* Imports documents from a folder structure.
		* Imports a bunch of documents from a given path, building the
		* folder tree from the found directory structure. This directory
		* must be valid and readable from the point of the service process.
		* The caller must be a valid admin user, the service has to be paused.
		*
		* @param sourcePath Absolute or relative path as seen by the service process.
		**/
		void importDocuments(string sourcePath) throws Common::ArchiveError;

		/**
		* Backs up an entire archive.
		* Performs a backup of all storage files into a single storage
		* in a given directory. This directory must be valid
		* and writable from the point of the service process. An
		* already existing backup will be removed before running
		* the actual backup. The file will be named 'domla.archive.backup'.
		* The caller must be a valid admin user, the service does not have to but can be paused.
		*
		* @param targetPath Absolute or relative path as seen by the service process.
		**/
		void backup(string targetPath) throws Common::ArchiveError;

		/**
		* Restores an archive from a backup file.
		* Searches for a file named 'domla.archive.backup' in the
		* given directory and rebuilds the whole archive from it.
		* This operation overwrites all existing archive items.
		* The caller must be a valid admin user, the service has to be paused.
		*
		* @param sourcePath Absolute or relative path as seen by the service process.
		**/
		void restore(string sourcePath) throws Common::ArchiveError;

		/**
		* Pauses request handling except for the Commander.
		* This call blocks newly incoming requests, waits until all already
		* running requests are habe been processed and returns to the caller.
		**/
		void pauseService();

		/**
		* Resumes normal archive operations.
		* Moves the service state from paused to active.
		**/
		void resumeService();

		/**
		* State info for archive.
		* @return true = servicing is paused, false = servicing is not paused (active)
		**/
		bool isServicePaused();
	};
};