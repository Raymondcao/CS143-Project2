/**
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */
 
#include "Bruinbase.h"
#include "SqlEngine.h"
#include <cstdio>

#include "BTreeIndex.h"
#include "RecordFile.h"

int main()
{
  // run the SQL engine taking user commands from standard input (console).
  // SqlEngine::run(stdin);

  // RecordFile* rf = new RecordFile("testdata.o", 'w');
  BTreeIndex* tree = new BTreeIndex();
  // tree->open("test.o", 'w');
  tree->open("test.o", 'r');

  // RecordId rid;
  // for (int key =1; key< 10000; key++){
	 //  rf->append(key, "first test", rid);
  //   fprintf(stdout, "origin key: %i rid pid:%i sid:%i\n", key, rid.pid, rid.sid);
	 //  tree->insert(key, rid);
  // }

  tree->printTree();

  tree->close();

  return 0;
}
