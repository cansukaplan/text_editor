#include <curses.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>

#define TABSIZE 2
#define CHUNKSIZE 1024
#define STRMAX 64
#define BUFSIZE 256

#ifdef CTRL
#undef CTRL
#endif
#define CTRL(a) ((a) & 31)

char	*metin = 0;
int	metin_boyutu = 0;

char	dosya_adi[STRMAX] = "";

int	bas = 0, son = 0, imlec = 0, eof_pos = 0, satir_basi = 0;
int	bow_line_prev = 0, win_shift = 0, imlec_satir = 0, imlec_y = 0, imlec_x = 0;
int	is_changed = 0,  ins_mode = 1,  find_mode = 0;

#define	ALIGN(x,mod)	(((x) / (mod) + 1) * (mod))
#define nexttab(x)	ALIGN (x, TABSIZE)
#define align_chunk(x)	ALIGN (x, CHUNKSIZE)

int	onayla (char *s)
{
    int	ch;

    move (LINES - 1, 0);
    addstr (s);
    refresh ();
    ch = getch ();
}

int	dosyaAdiAl (char *s, char *buf)
{
    int	b_len, ch, flag = 1;

    for (;;)
    {
        move (LINES - 1, 0);
        addstr (s);
        for (b_len = 0; buf[b_len]; b_len++)
            addch (buf[b_len]);
        refresh ();
        ch = getch ();
        switch (ch)
        {
        case CTRL ('Y'):
            *buf = 0;
            break;
        case 8:
            if (b_len)
                buf[b_len - 1] = 0;
            break;
        case '\r':
            return 1;
        case CTRL ('X'):
            return 0;
        default:
            if (!iscntrl (ch))
            {
ins_char:
                if (flag)
                    *buf = b_len = 0;
                if (b_len < STRMAX - 1)
                {
                    buf[b_len] = ch;
                    buf[b_len + 1] = 0;
                }
            }
            else
                beep ();
            break;
        }
        flag = 0;
    }
}

int	hata (char *s, ...)
{
    va_list	args;
    char	buf[BUFSIZE];
    int	i = 0;

    va_start (args, s);
    i += snprintf (buf + i, BUFSIZE - i, "Hata ");
    va_end (args);
    beep ();
    onayla (buf);

    return 0;
}

int	sil (int pos)
{
    while (pos && metin[pos - 1] != '\n')
        pos--;
    return pos;
}

int	satir (int pos)
{
    pos = sil (pos);
    return pos ? sil (pos - 1) : 0;
}

int	alt_kontrol (int pos)
{
    while (pos < eof_pos && metin[pos] != '\n')
        pos++;
    return pos;
}

int	altsatir (int pos)
{
    pos = alt_kontrol (pos);
    return pos < eof_pos ? pos + 1 : pos;
}

int	duzelt (int line, int xx)
{
    int	i, x = 0;

    for (i = line; i < eof_pos && i < line + xx; i++)
        if (metin[i] == '\n')
            break;
        else if (metin[i] == '\t')
            x = nexttab (x);
        else
            x++;
    return x;
}

int	pos_x (int line, int xx)
{
    int	i, x = 0;

    for (i = line; i < eof_pos && x < xx; i++)
        if (metin[i] == '\n')
            break;
        else if (metin[i] == '\t')
            x = nexttab (x);
        else
            x++;
    return i;
}

void	goster (void)
{
    int	i, m, t, j;

    if (satir_basi > bow_line_prev)
    {
        m = bow_line_prev;
        for (i = 0; m != satir_basi && i < LINES; i++)
            m = altsatir (m);
        if (i < LINES)
            scrl (i);
    }
    else if (satir_basi < bow_line_prev)
    {
        m = bow_line_prev;
        for (i = 0; m != satir_basi && i < LINES; i++)
            m = satir (m);
        if (i < LINES)
            scrl (-i);
    }
    bow_line_prev = satir_basi;

    erase ();
    if (!metin)
        return;
    for (m = satir_basi, i = 0; m < eof_pos && i < LINES; i++)
    {
        m = pos_x (m, win_shift);
        move (i, 0);
#define EOS_COLS (i < LINES - 1 ? COLS : COLS - 1)
        for (j = 0; m < eof_pos && j < EOS_COLS; m++)
        {
            if (m >= bas && m < son)
                attron (A_REVERSE);
            else
                attroff (A_REVERSE);
            if (metin[m] == '\n')
                break;
            else if (metin[m] == '\t')
                for (t = nexttab (j); j < t; j++)
                    addch (' ');
            else
            {
                addch (metin[m]);
                j++;
            }
        }
        if (m >= bas && m < son)
            while (j++ < EOS_COLS)
                addch (' ');
#undef EOS_COLS
        m = altsatir (m);
    }
    attroff (A_REVERSE);
}

void	yukari (void)
{
    imlec_satir = satir (imlec_satir);
    imlec= pos_x (imlec_satir,imlec_x + win_shift);
}

void	asagi (void)
{
    if (alt_kontrol (imlec) < eof_pos)
    {
        imlec_satir = altsatir (imlec_satir);
        imlec = pos_x (imlec_satir, imlec_x + win_shift);
    }
}

int	dosya_kontrol (int boyut)
{
    char	*p;
    int	i;

    if (!metin || eof_pos + boyut > metin_boyutu)
    {
        i = align_chunk (eof_pos + boyut);
        p = realloc (metin, i);
        if (!p)
            return hata ("- ram de yer yok");
        metin = p;
        metin_boyutu = i;
    }
    for (i = eof_pos - 1; i >= imlec; i--)
        metin[i + boyut] = metin[i];
    eof_pos += boyut;
    if (bas >= imlec)
        bas += boyut;
    if (son > imlec)
        son += boyut;
    is_changed = 1;
    return 1;
}
void	bellek (int pos, int size)
{
    int	i;
    char	*p;

    for (i = pos + size; i < eof_pos; i++)
        metin[i - size] = metin[i];
    eof_pos -= size;
    is_changed = 1;
#define del_pos(p) (p > pos + size ? p -= size : p > pos ? p = pos : p)
    del_pos (bas);
    del_pos (son);
    del_pos (imlec);
    del_pos (satir_basi);
    del_pos (bow_line_prev);
#undef del_pos
    i = align_chunk (eof_pos);
    if (i < metin_boyutu)
    {
        p = realloc (metin, i);
        if (!p)
        {
            hata ("- realloc yaparken sikinti oldu");
            return;
        }
        metin = p;
        metin_boyutu = i;
    }
}

void	insert(char ch)
{
    if (  !ins_mode && imlec < eof_pos)
    {
        if (ch == '\n')
        {
            imlec = altsatir (imlec);
            return;
        }
        else if (metin[imlec] != '\n')
        {
            is_changed = 1;
            goto a;
        }
    }
    if (dosya_kontrol (1))
a:
        metin[imlec++] = ch;
}

void	kopyalama (void)
{
    if (son <= bas || (imlec > bas && imlec < son))
        beep ();
    else if (dosya_kontrol (son - bas))
        strncpy (metin + imlec, metin + bas, son - bas);
}
void	silme (void)
{
    if (son <= bas)
        beep ();
    else
        bellek (bas, son - bas);
}
void	kesme (void)
{
    int	i;

    if (son <= bas || (imlec > bas && imlec < son))
    {
        beep ();
        return;
    }
    kopyalama ();
    i = son - bas;
    bellek (bas, i);
    bas = imlec;
    son = imlec + i;
}
int	yukle (char *name)
{
    FILE	*f;
    int	i,j;

    f = fopen (name, "r");
    if (!f)
        return hata ("$load file \"%s\"", name);
    if (fseek (f, 0, SEEK_END))
        return hata ("$seek");
    i = ftell (f);
    j=i;
    if (dosya_kontrol (i))
    {
        if (fseek (f, 0, SEEK_SET))
            return hata ("$seek");
        if (fread (metin + imlec, 1, j, f) < i)
            return hata ("$read");
    }
    else
        i = 0;
    fclose (f);
    return i;
}

int	kaydet (char *name, int pos, int size)
{
    FILE	*f;

    f = fopen (name, "w");
    if (!f)
        return hata ("$save file \"%s\"", name);
    if (fwrite (metin, 1, size, f) < size)
        return hata ("$write");
    if (fclose (f))
        return hata ("$close");
    return 1;
}
void	k_kaydet (void)
{
    if (!dosyaAdiAl ("Kayit Etmek Icin Dosya Adini Giriniz: ", dosya_adi))
        return;
    if (kaydet (dosya_adi, 0, eof_pos))
        is_changed = 0;
}

void	exit (int sig)
{
    endwin ();
    exit (0);
}


void	cursesiAyarla (void)
{
    signal (SIGINT, exit);
    initscr ();

    keypad (stdscr, TRUE);
    scrollok (stdscr, TRUE);
    idlok (stdscr, TRUE);
    nonl ();
    raw ();
    noecho ();
}

void	ayarlama (void)
{
    int	i;

    imlec_satir = sil (imlec);
    while (imlec_satir < satir_basi)
        satir_basi = satir (satir_basi);
    imlec_y = 0;
    for (i = satir_basi; i < imlec_satir; i = altsatir (i))
        imlec_y++;
    for (;imlec_y >= LINES; imlec_y--)
        satir_basi = altsatir (satir_basi);
    imlec_x = duzelt (imlec_satir, imlec - imlec_satir) - win_shift;
    while (imlec_x < 0)
    {
        imlec_x += TABSIZE;
        win_shift -= TABSIZE;
    }
    while (imlec_x >= COLS)
    {
        imlec_x -= TABSIZE;
        win_shift += TABSIZE;
    }
}

int	main (int argv, char **argc)
{
    int	 ch;

    cursesiAyarla ();
    if (argv >= 2)
    {
        strncpy (dosya_adi, argc[1], STRMAX - 1);
        dosya_adi[STRMAX - 1] = 0;
        yukle (dosya_adi);
        is_changed = 0;
    }

    onayla ("Kayit Defteri\n-Yon Tuslari Ile Yazi Icinde Gezinti Yapabilirsiniz\n-CTRL-y -> satiri siler\n-Kopyalamak veya kesmek istedigiz alani CTRL-b ile baslayip CTRL-e tusu ile bitirerek secebilirsiniz\n-Sectiginiz alani kopyala/yapistir için CTRL-c , kes/yapistir icin CTRL-v tusunu kullanabilirsiniz\n-CTRL-s ile yazdiklarinizi kayit edebilirsiniz\n-CTRL-x ile uygulamadan cikabilirsiniz...");
    for (;;)
    {
        goster ();
        move (imlec_y, imlec_x);
        refresh ();
        ch = getch ();
        switch (ch)
        {
        case KEY_UP:
            yukari ();
            break;
        case KEY_DOWN:
            asagi ();
            break;
        case KEY_LEFT:
            if (imlec)
                imlec--;
            break;
        case KEY_RIGHT:
            if (imlec < eof_pos)
                imlec++;
            break;
        case KEY_DC:
            if (imlec < eof_pos)
                bellek (imlec, 1);
            break;

        case 8:
            if (imlec)
                bellek (--imlec, 1);
            break;
        case CTRL ('Y'):
            bellek (imlec_satir, altsatir (imlec_satir) - imlec_satir);
            break;
        case CTRL ('B'):
            bas = imlec;
            break;
        case CTRL ('E'):
            son = imlec;
            break;
        case CTRL ('C'):
            kopyalama ();
            break;
        case CTRL('D'):
            silme();
            break;
        case CTRL ('V'):
            kesme();
            break;
        case CTRL ('S'):
            k_kaydet ();
            break;
        case CTRL ('X'):
            if (!is_changed || onayla ("Kayit Etmeden Cikmak Istedigine Eminmisin? (e/h):"))
                exit (0);
            break;
        case '\r':
            ch = '\n';
        default:
            if (!iscntrl (ch) || ch == '\t' || ch == '\n')
                insert (ch);
            else
                beep ();
            break;
        }
        ayarlama ();
    }
    return 0;
}
