#ifndef __REPOS_H
#define __REPOS_H

extern LIST_ENTRY RepositCcbQueue;

NTSTATUS InsertToReposite(PCCB Ccb);
NTSTATUS ExecuteReposite(PLURELATION_NODE Lur);

#endif