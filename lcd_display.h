#ifndef	lcd_display_h
#define	lcd_display_h

#ifdef	lcd_display_c
		int     __lcd_inuse = 0;
#else

extern	int     __lcd_inuse;
#endif

void lcd_set_cursor(int line, int position);
void lcd_string(const char *s);

void	lcd_init(void);

void    lcd_off(void);
void    lcd_on(void);
int		lcd_toggle(void);

#endif