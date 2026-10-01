#ifndef AEGIS_PACKAGE_EXECUTION_PROTOCOL_H
#define AEGIS_PACKAGE_EXECUTION_PROTOCOL_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#define AEGIS_PACKAGE_EXEC_MAGIC UINT32_C(0x41455045)
#define AEGIS_PACKAGE_EXEC_VERSION 6u
#define AEGIS_PACKAGE_RECONCILIATION_ROOT_BYTES 66049u
#define AEGIS_PACKAGE_CHOICES_BYTES 17408u
#define AEGIS_PACKAGE_EXEC_ITEMS 64u
#define AEGIS_PACKAGE_EXEC_NAME 160u
#define AEGIS_PACKAGE_EXEC_FDS 3u
#define AEGIS_PACKAGE_ARCHIVES 1u
#define AEGIS_PACKAGE_REMOVE 2u
#define AEGIS_PACKAGE_MIXED 3u
#define AEGIS_PACKAGE_SELECTION 4u // private metadata only, no APT package action
#define AEGIS_PACKAGE_RECONCILE 5u // private base transition, including zero package effects
#define AEGIS_PACKAGE_EXEC_READY 1u
#define AEGIS_PACKAGE_EXEC_DONE 2u
struct aegis_package_expected_effect {
    char name[129], architecture[8], before[129], after[129];
    uint8_t reason; // 1 manual, 2 automatic (initial reason for removals).
};
struct aegis_package_execution_review {
    uint32_t present, apt_state_presence; // absent review is developer fixture only
    uint64_t apt_state_bytes;
    char initial_status[65], initial_apt_state[65];
    uint8_t reserved[6];
    char initial_choices[AEGIS_PACKAGE_CHOICES_BYTES], result_choices[AEGIS_PACKAGE_CHOICES_BYTES];
    struct aegis_package_expected_effect effects[AEGIS_PACKAGE_EXEC_ITEMS];
    char reconciliation_roots[AEGIS_PACKAGE_RECONCILIATION_ROOT_BYTES];
    char result_registry[65], result_automatic[65];
    uint8_t reconciliation_reserved[5];
};
struct aegis_package_execution_request {
    uint32_t magic, version, user, serial;
    uint64_t job;
    uint32_t kind, count;
    char plan[65];
    uint8_t reserved[7];
    char items[AEGIS_PACKAGE_EXEC_ITEMS][AEGIS_PACKAGE_EXEC_NAME];
    struct aegis_package_execution_review review;
};
struct aegis_package_execution_reply {
    uint32_t magic, version, user, serial;
    uint64_t job;
    char plan[65];
    uint8_t reserved[3];
    uint32_t phase, status, error;
};
static inline int aegis_package_zero(const void *data, size_t count) {
    const unsigned char *bytes = (const unsigned char *)data;
    for (size_t i = 0; i < count; i++) if (bytes[i]) return 0;
    return 1;
}
static inline int aegis_package_hash(const char hash[65]) {
    if (hash[64]) return 0;
    for (unsigned i = 0; i < 64; i++)
        if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f'))) return 0;
    return 1;
}
static inline int aegis_package_item(const char name[AEGIS_PACKAGE_EXEC_NAME], uint32_t kind) {
    size_t length = strnlen(name, AEGIS_PACKAGE_EXEC_NAME);
    if (length < 2 || length == AEGIS_PACKAGE_EXEC_NAME
            || !aegis_package_zero(name + length, AEGIS_PACKAGE_EXEC_NAME - length)) return 0;
    if (!(name[0] >= 'a' && name[0] <= 'z') && !(name[0] >= '0' && name[0] <= '9')) return 0;
    for (size_t i = 0; i < length; i++) {
        char c = name[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.') continue;
        if (kind == AEGIS_PACKAGE_ARCHIVES
                && ((c >= 'A' && c <= 'Z') || c == '_' || c == '~' || c == ':' || c == '%')) continue;
        return 0;
    }
    // APT interprets a trailing +/- as an action override. Until the planner
    // supplies an unambiguous qualified form, reject those package names.
    if (kind == AEGIS_PACKAGE_REMOVE && (name[length - 1] == '+' || name[length - 1] == '-')) return 0;
    return kind == AEGIS_PACKAGE_REMOVE || (length > 4 && !strcmp(name + length - 4, ".deb"));
}
static inline int aegis_package_fixed(const char *text,size_t size,int empty) {
    size_t n=strnlen(text,size);
    if(n==size || (!empty && !n) || !aegis_package_zero(text+n,size-n))return 0;
    for(size_t i=0;i<n;++i)if((unsigned char)text[i]<33 || (unsigned char)text[i]>126)return 0;
    return 1;
}
static inline int aegis_package_choices_text(const char *text) {
    size_t n=strnlen(text,AEGIS_PACKAGE_CHOICES_BYTES);
    if(n==AEGIS_PACKAGE_CHOICES_BYTES || !aegis_package_zero(text+n,AEGIS_PACKAGE_CHOICES_BYTES-n))return 0;
    for(size_t i=0;i<n;++i)if((text[i]<32 && text[i]!='\n' && text[i]!='\t') || (unsigned char)text[i]>126)return 0;
    return 1;
}
static inline int aegis_package_review_valid(const struct aegis_package_execution_request *r) {
    const struct aegis_package_execution_review *v=&r->review;
    if(!v->present)return r->kind!=AEGIS_PACKAGE_MIXED && r->kind!=AEGIS_PACKAGE_SELECTION && r->kind!=AEGIS_PACKAGE_RECONCILE && aegis_package_zero(v,sizeof(*v));
    if(v->present!=1 || !aegis_package_hash(v->initial_status)
       || !aegis_package_zero(v->reserved,sizeof(v->reserved))
       || !aegis_package_zero(v->reconciliation_reserved,sizeof(v->reconciliation_reserved)))return 0;
    if(!aegis_package_choices_text(v->initial_choices) || !aegis_package_choices_text(v->result_choices)
       || (*v->initial_choices && !*v->result_choices))return 0;
    if(r->kind==AEGIS_PACKAGE_RECONCILE) {
        if(!*v->initial_choices || strcmp(v->initial_choices,v->result_choices)
           || !aegis_package_hash(v->result_registry) || !aegis_package_hash(v->result_automatic))return 0;
        size_t n=strnlen(v->reconciliation_roots,sizeof(v->reconciliation_roots));
        if(!n || n==sizeof(v->reconciliation_roots)
           || !aegis_package_zero(v->reconciliation_roots+n,sizeof(v->reconciliation_roots)-n))return 0;
        for(size_t i=0;i<n;++i)if(v->reconciliation_roots[i]!='\n'
            && ((unsigned char)v->reconciliation_roots[i]<33 || (unsigned char)v->reconciliation_roots[i]>126))return 0;
    } else if(!aegis_package_zero(v->reconciliation_roots,sizeof(v->reconciliation_roots))
              ||!aegis_package_zero(v->result_registry,65)||!aegis_package_zero(v->result_automatic,65))return 0;
    if(r->kind==AEGIS_PACKAGE_SELECTION && (r->count!=1 || !*v->result_choices
       || !strcmp(v->initial_choices,v->result_choices)))return 0;
    if(v->apt_state_presence==1) {
        if(v->apt_state_bytes || !aegis_package_zero(v->initial_apt_state,65))return 0;
    } else if(v->apt_state_presence!=2 || v->apt_state_bytes>(UINT64_C(16)<<20)
              || !aegis_package_hash(v->initial_apt_state))return 0;
    for(unsigned i=0;i<AEGIS_PACKAGE_EXEC_ITEMS;++i) {
        const struct aegis_package_expected_effect *e=&v->effects[i];
        if(i>=r->count) { if(!aegis_package_zero(e,sizeof(*e)))return 0;continue; }
        if(!aegis_package_fixed(e->name,sizeof(e->name),0)
           || !aegis_package_fixed(e->architecture,sizeof(e->architecture),0)
           || (strcmp(e->architecture,"all") && strcmp(e->architecture,"arm64"))
           || !aegis_package_fixed(e->before,sizeof(e->before),1)
           || !aegis_package_fixed(e->after,sizeof(e->after),1)
           || (r->kind==AEGIS_PACKAGE_SELECTION
               ? !*e->before || strcmp(e->before,e->after) || e->reason!=1
               : !strcmp(e->before,e->after))
           || (e->reason!=1 && e->reason!=2)
           || (i && strcmp(v->effects[i-1].name,e->name)>=0)
           || (r->kind==AEGIS_PACKAGE_REMOVE && *e->after)
           || (r->kind==AEGIS_PACKAGE_ARCHIVES && !*e->after))return 0;
    }
    return 1;
}
// The caller validates framing/review before relying on the effect mapping.
static inline int aegis_package_has_archive(const struct aegis_package_execution_request *r, unsigned i) {
    return i<r->count && i<AEGIS_PACKAGE_EXEC_ITEMS && (r->kind==AEGIS_PACKAGE_ARCHIVES
        || ((r->kind==AEGIS_PACKAGE_MIXED || r->kind==AEGIS_PACKAGE_RECONCILE) && r->review.effects[i].after[0]));
}
static inline unsigned aegis_package_archive_count(const struct aegis_package_execution_request *r) {
    unsigned count=0;
    for(unsigned i=0;i<r->count && i<AEGIS_PACKAGE_EXEC_ITEMS;++i)count+=aegis_package_has_archive(r,i);
    return count;
}
static inline int aegis_package_execution_valid(const struct aegis_package_execution_request *r) {
    if (r->magic != AEGIS_PACKAGE_EXEC_MAGIC || r->version != AEGIS_PACKAGE_EXEC_VERSION
            || r->user < 10 || r->user >= 21473 || r->serial > INT32_MAX
            || !r->job || r->job > INT64_MAX || !aegis_package_hash(r->plan)
            || !aegis_package_zero(r->reserved, sizeof(r->reserved))
            || (r->kind != AEGIS_PACKAGE_ARCHIVES && r->kind != AEGIS_PACKAGE_REMOVE && r->kind != AEGIS_PACKAGE_MIXED && r->kind != AEGIS_PACKAGE_SELECTION && r->kind != AEGIS_PACKAGE_RECONCILE)
            || (!r->count && r->kind!=AEGIS_PACKAGE_RECONCILE) || r->count > AEGIS_PACKAGE_EXEC_ITEMS || !aegis_package_review_valid(r)) return 0;
    for (unsigned i = 0; i < AEGIS_PACKAGE_EXEC_ITEMS; i++) {
        if (i >= r->count) { if (!aegis_package_zero(r->items[i], sizeof(r->items[i]))) return 0;continue; }
        int archive=aegis_package_has_archive(r,i);
        if (!aegis_package_item(r->items[i], archive ? AEGIS_PACKAGE_ARCHIVES : AEGIS_PACKAGE_REMOVE)) return 0;
        if (!archive && r->review.present && strcmp(r->items[i],r->review.effects[i].name)) return 0;
        for (unsigned j = 0; j < i; j++) if (!strcmp(r->items[i], r->items[j])) return 0;
    }
    unsigned archives=aegis_package_archive_count(r);
    return r->kind!=AEGIS_PACKAGE_MIXED || (archives>0 && archives<r->count);
}
#ifdef __cplusplus
extern "C" {
#endif
/* Private committed transfer: readonly sealed request + candidate/device
 * mounts from the owning broker. Sender retains its FDs. No CLI endpoint. */
int aegis_package_execution_send(int channel, uint32_t user, uint32_t serial, uint64_t job,
                                 const int fds[AEGIS_PACKAGE_EXEC_FDS]);
/* Receiver retains partial received FDs on failure; caller must close them. */
int aegis_package_execution_receive(int channel, uint32_t user, uint32_t serial,
                                    int fds[3], struct aegis_package_execution_request *request);
/* Trusted namespace PID1 core; production entry separately requires the exact
 * package SELinux domain. This never takes an arbitrary command or script. */
int aegis_package_execute(uint32_t user, uint32_t serial, int permit_unbound_fixture);
#ifdef __cplusplus
}
#endif
#endif
