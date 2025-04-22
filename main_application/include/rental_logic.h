#ifndef RENTAL_LOGIC_H
#define RENTAL_LOGIC_H

typedef struct {
    char tag_id[32];
    uint32_t start_time;
    uint32_t duration;
    bool active;
} rental_data_t;

int process_rental(const char *tag_id);
int check_rental_expiration(void);

#endif /* RENTAL_LOGIC_H */
