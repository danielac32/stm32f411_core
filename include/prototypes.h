extern	void	platinit(void);
extern interrupt TIM2_Handler();

/* in file clkinit.c */
extern	void	clkinit(void);


typedef char buf_t[128];
//buf_t path, curdir;

extern void update_path();
extern char* full_path(const char* name);
extern buf_t path, curdir;

extern	syscall	freemem(char *, uint32);
extern	char	*getmem(uint32);
extern	void	halt(void);