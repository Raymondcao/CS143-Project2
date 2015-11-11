#include "BTreeNode.h"

using namespace std;

const int MAX_NODE_SIZE = 84;
const int ENTRY_SIZE = sizeof(int)+sizeof(RecordId);
const int BUFFER_SIZE = PageFile::PAGE_SIZE;

// For each node, we create a integer in beginning of the node buffer to record node entry count. 
// At the end of the buffer, there is a pageid pointing to the next page.
// ----------------------------------------------------------------------------
// |--count(4 bytes)--|--node entries(12 bytes for each)--|--pageid(4 bytes)--|
// ----------------------------------------------------------------------------

// For each entry of a node in buffer,
// ----------------------------------------
// |--key(4 bytes)--|--RecordId(8 bytes)--|
// ----------------------------------------


/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::read(PageId pid, const PageFile& pf)
{
	return pf.read(pid, buffer);
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::write(PageId pid, PageFile& pf)
{
	return pf.write(pid, buffer);
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTLeafNode::getKeyCount()
{
	int count;
	memcpy(&count, buffer, sizeof(count));
	return count;
}

/*
 * Insert a (key, rid) pair to the node.
 * @param key[IN] the key to insert
 * @param rid[IN] the RecordId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTLeafNode::insert(int key, const RecordId& rid)
{
	int count = getKeyCount();

	if (count==MAX_NODE_SIZE)
		return RC_NODE_FULL;

	int startPos = sizeof(count);
	int i;
	if (locate(key,i)<0){
		startPos += count*ENTRY_SIZE;
	}else{
		startPos += i*ENTRY_SIZE;
	}

	// move other entries and insert the new entry
	memcpy(buffer+startPos+ENTRY_SIZE, buffer+startPos, (count-i)*ENTRY_SIZE);
	memcpy(buffer+startPos, &key, sizeof(key));
	memcpy(buffer+startPos+sizeof(key), &rid, sizeof(rid));

	// update count;
	count++;
	memcpy(buffer, &count, sizeof(count));

	return 0;
}

/*
 * Insert the (key, rid) pair to the node
 * and split the node half and half with sibling.
 * The first key of the sibling node is returned in siblingKey.
 * @param key[IN] the key to insert.
 * @param rid[IN] the RecordId to insert.
 * @param sibling[IN] the sibling node to split with. This node MUST be EMPTY when this function is called.
 * @param siblingKey[OUT] the first key in the sibling node after split.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::insertAndSplit(int key, const RecordId& rid, 
                              BTLeafNode& sibling, int& siblingKey)
{
	int count = getKeyCount();

	int eid;
	locate(key,eid);

	int leftSize = (count+2)/2;
	int rightSize = count+1 - leftSize;

	int curKey;
	RecordId curRid;
	if (eid<leftSize){
		while(count> leftSize-1){
			readEntry(--count, curKey, curRid);
			sibling.insert(curKey, curRid);
		}
		insert(key, rid);
	}else{
		while(count> leftSize){
			readEntry(--count, curKey, curRid);
			sibling.insert(curKey, curRid);
		}
		sibling.insert(key, rid);
	}

	// update count
	memcpy(buffer, &count, sizeof(count));

	if (sibling.readEntry(0, curKey, curRid)<0)return RC_END_OF_TREE;
	siblingKey = curKey;

	return 0;
}

/**
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTLeafNode::locate(int searchKey, int& eid)
{
	int count = getKeyCount();

	for(int i=0; i<count; i++){
		int curKey;
		memcpy(&curKey, buffer, sizeof(curKey));
		
		if (curKey>=key){
			eid = i;
			return 0;
		}
	}

	return RC_NO_SUCH_RECORD;
}

/*
 * Read the (key, rid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param rid[OUT] the RecordId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::readEntry(int eid, int& key, RecordId& rid)
{
	int count = getKeyCount();

	if (eid>=count)return RC_END_OF_TREE;
	memcpy(&key, buffer+sizeof(count)+ENTRY_SIZE*eid, sizeof(key));
	memcpy(&rid, buffer+sizeof(count)+sizeof(key)+ENTRY_SIZE*eid, sizeof(RecordId));
	return 0;
}

/*
 * Return the pid of the next slibling node.
 * @return the PageId of the next sibling node 
 */
PageId BTLeafNode::getNextNodePtr()
{
	PageId pid;
	memcpy(&pid, buffer+BUFFER_SIZE-sizeof(pid), sizeof(pid));
	return pid;
}

/*
 * Set the pid of the next slibling node.
 * @param pid[IN] the PageId of the next sibling node 
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid)
{
	memcpy(buffer+BUFFER_SIZE-sizeof(pid), &pid, sizeof(pid));
	return 0;
}

/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::read(PageId pid, const PageFile& pf)
{ 
	pf.read(pid, buffer);
	return 0; 
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::write(PageId pid, PageFile& pf)
{ 
	pf.write(pid, buffer);
	return 0; 
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount()
{ return 0; }


/*
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid)
{
	return 0; 
}

/*
 * Insert the (key, pid) pair to the node
 * and split the node half and half with sibling.
 * The middle key after the split is returned in midKey.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @param sibling[IN] the sibling node to split with. This node MUST be empty when this function is called.
 * @param midKey[OUT] the key in the middle after the split. This key should be inserted to the parent node.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::insertAndSplit(int key, PageId pid, BTNonLeafNode& sibling, int& midKey)
{ return 0; }

/*
 * Given the searchKey, find the child-node pointer to follow and
 * output it in pid.
 * @param searchKey[IN] the searchKey that is being looked up.
 * @param pid[OUT] the pointer to the child node to follow.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::locateChildPtr(int searchKey, PageId& pid)
{ return 0; }

/*
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2)
{ return 0; }
