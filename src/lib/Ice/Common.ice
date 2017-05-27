#pragma once

module Common {
	// Archive specific exceptions
	exception ArchiveError {
		string reason;
	};
    
	exception UnexpectedError extends ArchiveError {
		string innerError;
	};
    
	class Persistable {
        string Id;
    };
};