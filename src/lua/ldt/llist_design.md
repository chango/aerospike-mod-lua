-- Large Ordered List (llist.lua)
-- Track the date and iteration of the last update:
local MOD = "llist_2013_10_08.a";

-- This variable holds the version of the code (Major.Minor).
-- We'll check this for Major design changes -- and try to maintain some
-- amount of inter-version compatibility.
local G_LDT_VERSION = 1.1;

-- ======================================================================
-- || GLOBAL PRINT ||
-- ======================================================================
-- Use this flag to enable/disable global printing (the "detail" level
-- in the server).  We may also use "F" as a general guard for larger
-- print debug blocks -- as well as the individual trace/info lines.
-- ======================================================================
local GP=true; -- Doesn't matter what this value is.
local F=true; -- Set F (flag) to true to turn ON global print
local E=true; -- Set F (flag) to true to turn ON Enter/Exit print
local B=true; -- Set B (Banners) to true to turn ON Banner Print

-- ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
-- <<  LLIST Main Functions >>
-- ||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
-- The following external functions are defined in the LLIST module:
--
-- (*) Status = add( topRec, ldtBinName, newValue, userModule )
-- (*) Status = add_all( topRec, ldtBinName, valueList, userModule )
-- (*) List   = find( topRec, ldtBinName, searchValue ) 
-- (*) List   = scan( topRec, ldtBinName )
-- (*) List   = filter( topRec, ldtBinName, userModule, filter, fargs )
-- (*) Status = remove( topRec, ldtBinName, searchValue ) 
-- (*) Status = destroy( topRec, ldtBinName )
-- (*) Number = size( topRec, ldtBinName )
-- (*) Map    = get_config( topRec, ldtBinName )
-- (*) Status = set_capacity( topRec, ldtBinName, new_capacity)
-- (*) Status = get_capacity( topRec, ldtBinName )
-- ======================================================================

-- TODO (Major Feature Items:  (N) Now, (L) Later
-- (N) Switch all Lua External functions to return two-part values, which
--     Need to match what the new C API expects (when Chris returns).
-- (N) Handle Duplicates (search, Scan, Delete)
-- (L) Vector Operations for Insert, Search, Scan, Delete
--     ==> A LIST of Operations to perform, along with a LIST of RESULT
--         to return.
-- (L) Change the SubRec Context to close READONLY pages when we're done
--     with them -- and keep open ONLY the dirty pages.  So, we have to mark
--     dirty pages in the SRC.  We could manage the SRC like a buffer pool
--     that closes oldest READONLY pages when it needs space.
-- (L) Build the tree from the list, using "buildTree()" method, rather
--     than individual inserts.  Sorted list is broken into leaves, which
--     become tree leaves.  Allocate parents as necessary.  Build bottom
--     up.
-- TODO (Minor Design changes, Adjustments and Fixes)
-- TODO (Testing)
-- (*) Fix "leaf Count" in the ldt map
-- (*) Test/validate Simple delete
-- (*) Test/validate Simple scan
-- (*) Complex Insert
-- (*) Complex Search
-- (*) Complex delete
-- (*) Tree Delete (Remove)
-- (*) Switch CompactList to Sorted List (like the leaf list)
-- (*) Switch CompactList routines to use the "Leaf" List routines
--     for search, insert, delete and scan.
--     Search: Return success and position
--     Insert: Search, plus listInsert
--     Delete: Search, plus listDelete
--     Scan:   Search, plus listScan
-- (*) Test that Complex Type and KeyFunction is defined on create,
--     otherwise error.  Take no default action.
--
-- Large List contains a homogeneous list.  Although the objects may be
-- different sizes and shapes, the objects must be all the same type.
-- Furthermore, if the objects are a complex type, then the subset that is
-- used to order the objects must all be the same atomic type.
--     
-- Large List API
--
-- add(rec, bin, value): Add a value to the list
-- add_all(rec, bin, value_list )
-- find(rec, bin, value)
--    can be partial value (ie the index'd portion)
--    return a list of values matching the search value. or empty list
--    if no match.
-- findany(rec, bin, value)
--    return any (first) value matching the search value. or nil if no match.
-- getall(rec, bin)
-- filter(rec, bin, filter_function, args...)
-- range(rec, bin, lower, upper)
-- first(rec, bin, n) -- return first n items
-- last(rec, bin, n) -- return last n items
-- remove(rec, bin, value)
-- destroy(rec, bin)
-- size(rec, bin)
-- get_config(rec, bin)
--
-- DONE LIST
-- (*) Initialize Maps for Root, Nodes, Leaves
-- (*) Create Search Function
-- (*) Simple Insert (Root plus Leaf Insert)
-- (*) Complex Node Split Insert (Root and Inner Nodes)
-- (*) Simple Delete
-- (*) Simple Scan
-- ======================================================================
-- FORWARD Function DECLARATIONS
-- ======================================================================
-- We have some circular (recursive) function calls, so to make that work
-- we have to predeclare some of them here (they look like local variables)
-- and then later assign the function body to them.
-- ======================================================================
local insertParentNode;

-- ======================================================================
-- ++==================++
-- || External Modules ||
-- ++==================++
-- Set up our "outside" links.
-- Get addressability to the Function Table: Used for compress/transform,
-- keyExtract, Filters, etc. 
local functionTable = require('ldt/UdfFunctionTable');

-- When we're ready, we'll move all of our common routines into ldt_common,
-- which will help code maintenance and management.
-- local LDTC = require('ldt/ldt_common');

-- We import all of our error codes from "ldt_errors.lua" and we access
-- them by prefixing them with "ldte.XXXX", so for example, an internal error
-- return looks like this:
-- error( ldte.ERR_INTERNAL );
local ldte = require('ldt/ldt_errors');

-- We have a set of packaged settings for each LDT
local llistPackage = require('ldt/settings_llist');

-- ======================================================================
-- The Large Ordered List is a sorted list, organized according to a Key
-- value.  It is assumed that the stored object is more complex than just an
-- atomic key value -- otherwise one of the other Large Object mechanisms
-- (e.g. Large Stack, Large Set) would be used.  The cannonical form of a
-- LLIST object is a map, which includes a KEY field and other data fields.
--
-- In this first version, we may choose to use a FUNCTION to derrive the 
-- key value from the complex object (e.g. Map).
-- In the first iteration, we will use atomic values and the fixed KEY field
-- for comparisons.
--
-- Compared to Large Stack and Large Set, the Large Ordered List is managed
-- continuously (i.e. it is kept sorted), so there is some additional
-- overhead in the storage operation (to do the insertion sort), but there
-- is reduced overhead for the retieval operation, since it is doing a
-- binary search (order log(N)) rather than scan (order N).
-- ======================================================================
-- Large List Functions Supported
-- (*) create: Create the LLIST structure in the chosen topRec bin
-- (*) insert: Insert a user value (AS_VAL) into the list
-- (*) search: Search the ordered list, using tree search
-- (*) delete: Remove an element from the list
-- (*) scan:   Scan the entire tree
-- (*) remove: Remove the entire LDT from the record and remove bin.
-- ==> The Insert, Search and Delete functions have a "Multi" option,
--     which allows the caller to pass in multiple list keys that will
--     result in multiple operations.  Multi-operations provide higher
--     performance since there can be many operations performed with
--     a single "client-server crossing".
-- (*) insert_all():
-- (*) search_all():
-- (*) delete_all():
-- ==> The Insert and Search functions have the option of passing in a
--     Transformation/Filter UDF that modifies values before storage or
--     modify and filter values during retrieval.
-- (*) insert_with_udf() multi_insert_with_udf():
--     Insert a user value (AS_VAL) in the ordered list, 
--     calling the supplied UDF on the value FIRST to transform it before
--     storing it.
-- (*) search_with_udf, multi_search_with_udf:
--     Retrieve a value from the list. Prior to fetching the
--     item, apply the transformation/filter UDF to the value before
--     adding it to the result list.  If the value doesn't pass the
--     filter, the filter returns nil, and thus it would not be added
--     to the result list.
-- ======================================================================
-- LLIST Design and Type Comments:
--
-- The LLIST value is a new "particle type" that exists ONLY on the server.
-- It is a complex type (it includes infrastructure that is used by
-- server storage), so it can only be viewed or manipulated by Lua and C
-- functions on the server.  It is represented by a Lua MAP object that
-- comprises control information, a directory of records that serve as
-- B+Tree Nodes (either inner nodes or data nodes).
--
-- |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
-- |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
-- Here is a sample B+ tree:  There are N keys and (N+1) pointers (digests)
-- in an inner node (including the root).  All of the data resides in the
-- leaves, and the inner nodes are just keys and pointers.
-- Notice that real B+ Tree nodes have a fan-out of around 100 (maybe more,
-- maybe less, depending on key size), but that would be too hard to draw here.
--
--                                   _________
--             (Root Node)          |_30_|_60_|
--                               _/      |      \_
--                             _/        |        \_
--                           _/          |          \_
--                         _/            |            \_
--                       _/              |              \_
-- (internal nodes)    _/                |                \_
--          ________ _/          ________|              ____\_________
--         |_5_|_20_|           |_40_|_50_|            |_70_|_80_|_90_|
--        /    |    |          /     |    |           /     |    |     \
--       /     |    |         /      |    |          /      |    |     | 
--      /     /     |        /      /     |        _/     _/     |     |  
--     /     /      /       /      /      /       /      /      /      |   
--  +-^-++--^--++--^--+ +--^--++--^--++--^--+ +--^--++--^--++--^--++---^----+
--  |1|3||6|7|8||22|26| |30|39||40|46||51|55| |61|64||70|75||83|86||90|95|99|
--  +---++-----++-----+ +-----++-----++-----+ +-----++-----++-----++--------+
--  (Leaf Nodes)

-- The Root, Internal nodes and Leaf nodes have the following properties:
-- (1) The Root and Internal nodes store key values that may or may NOT
--     correspond to actual values in the leaf pages
-- (2) Key values and object values are stored in ascending order. 
--     We do not (yet) offer an ascending/descending order
-- (3) Root, Nodes and Leaves hold a variable number of keys and objects.
-- (4) Root, Nodes and Leaves may each have their own different capacity.
--
-- Searching a B+ tree is much like searching a binary
-- search tree, only the decision whether to go "left" or "right" is replaced
-- by the decision whether to go to child 1, child 2, ..., child n[x]. The
-- following procedure, B-Tree-Search, should be called with the root node as
-- its first parameter. It returns the block where the key k was found along
-- with the index of the key in the block, or "null" if the key was not found:
-- 
-- ++=============================================================++
-- || B-Tree-Search (x, k) -- search starting at node x for key k ||
-- ++=============================================================++
--     i = 1
--     -- search for the correct child
--     while i <= n[x] and k > keyi[x] do
--         i++
--     end while
-- 
--     -- now i is the least index in the key array such that k <= keyi[x],
--     -- so k will be found here or in the i'th child
-- 
--     if i <= n[x] and k = keyi[x] then 
--         -- we found k at this node
--         return (x, i)
--     
--     if leaf[x] then return null
-- 
--     -- we must read the block before we can work with it
--     Disk-Read (ci[x])
--     return B-Tree-Search (ci[x], k)
-- 
-- ++===========================++
-- || Creating an empty B+ Tree ||
-- ++===========================++
-- 
-- To initialize a B+ Tree, we build an empty root node, which means
-- we initialize the LListMap in topRec[LdtBinName].
--
-- Recall that we maintain a compact list of N elements (for values of N
-- usually between 20 and 50).  So, we always start with a group insert.
-- In fact, we'd prefer to take our initial list, then SORT IT, then
-- load directly into a leaf with the largest key in the leaf as the
-- first Root Value.  This initial insert sets up a special case where
-- there's a key value in the root, but only a single leaf, so there must
-- be a test to create the second leaf when the search value is >= the
-- single root key value.
-- 
-- This assumes there is an allocate-node function that returns a node with
-- key, c, leaf fields, etc., and that each node has a unique "address",
-- which, in our case, is an Aerospike record digest.
-- 
-- ++===============================++
-- || Inserting a key into a B-tree ||
-- ++===============================++
-- 
-- (*) Traverse the Tree, locating the Leaf Node that would contain the
-- new entry, remembering the path from root to leaf.
-- (*) If room in leaf, insert node.
-- (*) Else, split node, propagate dividing key up to parent.
-- (*) If parent full, split parent, propogate up. Iterate
-- (*) If root full, Create new level, move root contents to new level
--     NOTE: It might be better to divide root into 3 or 4 pages, rather
--     than 2.  This will take a little more thinking -- and the ability
--     to predict the future.
-- ======================================================================
-- Aerospike Server Functions:
-- ======================================================================
-- Aerospike Record Functions:
-- status = aerospike:create( topRec )
-- status = aerospike:update( topRec )
-- status = aerospike:remove( rec ) (not currently used)
--
-- Aerospike SubRecord Functions:
-- newRec = aerospike:create_subrec( topRec )
-- rec    = aerospike:open_subrec( topRec, digestString )
-- status = aerospike:update_subrec( childRec )
-- status = aerospike:close_subrec( childRec )
-- status = aerospike:remove_subrec( subRec ) 
--
-- Record Functions:
-- digest = record.digest( childRec )
-- status = record.set_type( topRec, recType )
-- status = record.set_flags( topRec, binName, binFlags )
--
-- ======================================================================
-- |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
-- || FUNCTION TABLE ||
-- |||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||
-- Table of Functions: Used for Transformation and Filter Functions.
-- This is held in UdfFunctionTable.lua.  Look there for details.
-- ===========================================
-- || GLOBAL VALUES -- Local to this module ||
-- ===========================================
-- ++====================++
-- || INTERNAL BIN NAMES || -- Local, but global to this module
-- ++====================++
-- The Top Rec LDT bin is named by the user -- so there's no hardcoded name
-- for each used LDT bin.
--
-- In the main record, there is one special hardcoded bin -- that holds
-- some shared information for all LDTs.
-- Note the 14 character limit on Aerospike Bin Names.
-- >> (14 char name limit) >>12345678901234<<<<<<<<<<<<<<<<<<<<<<<<<
local REC_LDT_CTRL_BIN    = "LDTCONTROLBIN"; -- Single bin for all LDT in rec

-- There are THREE different types of (Child) subrecords that are associated
-- with an LLIST LDT:
-- (1) Internal Node Subrecord:: Internal nodes of the B+ Tree
-- (2) Leaf Node Subrecords:: Leaf Nodes of the B+ Tree
-- (3) Existence Sub Record (ESR) -- Ties all children to a parent LDT
-- Each Subrecord has some specific hardcoded names that are used
--
-- All LDT subrecords have a properties bin that holds a map that defines
-- the specifics of the record and the LDT.
-- NOTE: Even the TopRec has a property map -- but it's stashed in the
-- user-named LDT Bin
-- >> (14 char name limit) >>12345678901234<<<<<<<<<<<<<<<<<<<<<<<<<
local SUBREC_PROP_BIN     = "SR_PROP_BIN";
--
-- The Node SubRecords (NSRs) use the following bins:
-- The SUBREC_PROP_BIN mentioned above, plus 3 of 4 bins
-- >> (14 char name limit) >>12345678901234<<<<<<<<<<<<<<<<<<<<<<<<<
local NSR_CTRL_BIN        = "NsrControlBin";
local NSR_KEY_LIST_BIN    = "NsrKeyListBin"; -- For Var Length Keys
local NSR_KEY_BINARY_BIN  = "NsrBinaryBin";-- For Fixed Length Keys
local NSR_DIGEST_BIN      = "NsrDigestBin"; -- Digest List

-- The Leaf SubRecords (LSRs) use the following bins:
-- The SUBREC_PROP_BIN mentioned above, plus
-- >> (14 char name limit) >>12345678901234<<<<<<<<<<<<<<<<<<<<<<<<<
local LSR_CTRL_BIN        = "LsrControlBin";
local LSR_LIST_BIN        = "LsrListBin";
local LSR_BINARY_BIN      = "LsrBinaryBin";

-- The Existence Sub-Records (ESRs) use the following bins:
-- The SUBREC_PROP_BIN mentioned above (and that might be all)
