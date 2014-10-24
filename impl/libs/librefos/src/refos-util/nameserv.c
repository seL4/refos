/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <refos-util/nameserv.h>
#include <data_struct/cvector.h>

/*! @file
    @brief Name server implementation library. */

static nameserv_entry_t*
nameserv_create_entry(const char* name, seL4_CPtr anonEP)
{
    if (!name || !anonEP) {
        return NULL;
    }
    uint32_t nameLen = strlen(name);
    if (nameLen == 0) {
        return NULL;
    }

    /* Allocate the nameserv entry. */
    nameserv_entry_t* entry = malloc(sizeof(nameserv_entry_t));
    if (!entry) {
        printf("WARNING: nameserv_create_entry failed to malloc entry. Out of memory.\n");
        return NULL;
    }

    /* Allocate name for the string. */
    entry->name = malloc(sizeof(char) * (nameLen + 2));
    if (!entry->name) {
        printf("WARNING: nameserv_create_entry failed to malloc name str. Out of memory.\n");
        free(entry);
        return NULL;
    }

    /* Fill in the data. */
    strcpy(entry->name, name);
    entry->magic = REFOS_NAMESERV_ENTRY_MAGIC;
    entry->anonEP = anonEP;

    return entry;
}

static void
nameserv_release_entry(nameserv_state_t *n, nameserv_entry_t* entry)
{
    assert(n && n->magic == REFOS_NAMESERV_MAGIC);
    assert(entry && entry->magic == REFOS_NAMESERV_ENTRY_MAGIC);
    assert(n->free_capability);

    entry->magic = 0;
    free(entry->name);
    n->free_capability(entry->anonEP);
    free(entry);
}

void
nameserv_init(nameserv_state_t *n, void (*free_cap) (seL4_CPtr cap))
{
    assert(n && free_cap);
    n->magic = REFOS_NAMESERV_MAGIC;
    n->free_capability = free_cap;
    cvector_init(&n->entries);
}

void
nameserv_release(nameserv_state_t *n)
{
    assert(n && n->magic == REFOS_NAMESERV_MAGIC);
    int nEntries = cvector_count(&n->entries);
    for (int i = 0; i < nEntries; i++) {
        nameserv_entry_t *nameEntry = (nameserv_entry_t *) cvector_get(&n->entries, i);
        nameserv_release_entry(n, nameEntry);
    }
    cvector_free(&n->entries);
    n->magic = 0;
}

int
nameserv_add(nameserv_state_t *n, const char* name, seL4_CPtr anonEP)
{
    assert(n && n->magic == REFOS_NAMESERV_MAGIC);
    if (!name || !anonEP) {
        return EINVALIDPARAM;
    }
    nameserv_delete(n, name);
    nameserv_entry_t *nameEntry = nameserv_create_entry(name, anonEP);
    if (!nameEntry) {
        return ENOMEM;
    }
    cvector_add(&n->entries, (cvector_item_t) nameEntry);
    return ESUCCESS;
}

static int
nameserv_find_entry_index(nameserv_state_t *n, const char* name)
{
    assert(n && n->magic == REFOS_NAMESERV_MAGIC);
    if (!name) {
        return -1;
    }
    int nEntries = cvector_count(&n->entries);
    /* Loop through the list and find a matching name. */
    for (int i = 0; i < nEntries; i++) {
        nameserv_entry_t *nameEntry = (nameserv_entry_t *) cvector_get(&n->entries, i);
        assert(nameEntry && nameEntry->name && nameEntry->magic == REFOS_NAMESERV_ENTRY_MAGIC);
        if (!strcmp(nameEntry->name, name)) {
            return i;
        }
    }
    return -1;
}

void
nameserv_delete(nameserv_state_t *n, const char* name)
{
    assert(n && n->magic == REFOS_NAMESERV_MAGIC);
    int idx = nameserv_find_entry_index(n, name);
    if (idx == -1) {
        return;
    }
    nameserv_entry_t *nameEntry = (nameserv_entry_t *) cvector_get(&n->entries, idx);
    nameserv_release_entry(n, nameEntry);
    cvector_delete(&n->entries, idx);
}

int
nameserv_resolve(nameserv_state_t *n, const char* path, seL4_CPtr *outAnonCap)
{
    assert(n && n->magic == REFOS_NAMESERV_MAGIC);

    /* Return no resolve on empty paths. */
    if (!path) {
        return 0;
    }
    int pathLen = strlen(path);
    if (pathLen <= 0) {
        return 0;
    }

    /* Ignore any leading slashes. */
    const char* path_ = path;
    if (path_[0] == '/') {
        path_++;
    }

    /* Allocate temporary buffer to store path segment. */
    char *pathSegment = malloc(sizeof(char) * (pathLen + 2));
    if (!pathSegment) {
        printf("ERROR: nameserv_resolve could not allocate path segment. OOM.\n");
        return 0;
    }
    int pathSegmentIndex = 0;

    /* Find the next slash-separated path segment. */
    bool segmentFound = false;
    const char* ci;
    for (ci = path_; *ci != '\0'; ci++) {
        if (*ci == '/') {
            segmentFound = true;
            break;
        }
        pathSegment[pathSegmentIndex++] = *ci;
    }
    pathSegment[pathSegmentIndex++] = '\0';

    /* If are at end of path resolvation, return our own anonymous endpoint. */
    if (!segmentFound) {
        return REFOS_NAMESERV_RESOLVED;
    }

    /* Otherwise, find the external anon endpoint. */
    int idx = nameserv_find_entry_index(n, pathSegment);
    if (idx == -1) {
        /* Name not found. */
        return 0;
    }

    /* External EP name resolvation succeeded. */
    nameserv_entry_t *nameEntry = (nameserv_entry_t *) cvector_get(&n->entries, idx);
    assert(nameEntry && nameEntry->name && nameEntry->magic == REFOS_NAMESERV_ENTRY_MAGIC);
    if (outAnonCap) {
        (*outAnonCap) = nameEntry->anonEP;
    }
    int offset = ci - path;
    assert(offset >= 0 && offset < pathLen);
    return offset;
}