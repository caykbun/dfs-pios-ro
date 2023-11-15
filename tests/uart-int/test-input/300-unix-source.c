0000: // thompson's complete original unix code.  it's an astonishing
0000: // monument to concise power.
0000: //
0000: // copied from:
0000: //    from lion's commentary: https://warsus.github.io/lions-/
0000: //
0000: // consider buying lion's commentary (recommended): 
0000: //
0000: // https://www.amazon.com/Lions-Commentary-Unix-John/dp/1573980137
0000: //
0000: ******************************************************************
0000:
0100: /* fundamental constants: cannot be changed */
0101: 
0102: 
0103: #define USIZE   16              /* size of user block (*64) */
0104: #define NULL    0
0105: #define NODEV   (-1)
0106: #define ROOTINO 1               /* i number of all roots */
0107: #define DIRSIZ  14              /* max characters per directory */
0108: 
0109: 
0110: /* signals: dont change */
0111: 
0112: 
0113: #define NSIG    20
0114: #define         SIGHUP  1       /* hangup */
0115: #define         SIGINT  2       /* interrupt (rubout) */
0116: #define         SIGQIT  3       /* quit (FS) */
0117: #define         SIGINS  4       /* illegal instruction */
0118: #define         SIGTRC  5       /* trace or breakpoint */
0119: #define         SIGIOT  6       /* iot */
0120: #define         SIGEMT  7       /* emt */
0121: #define         SIGFPT  8       /* floating exception */
0122: #define         SIGKIL  9       /* kill */
0123: #define         SIGBUS  10      /* bus error */
0124: #define         SIGSEG  11      /* segmentation violation */
0125: #define         SIGSYS  12      /* sys */
0126: #define         SIGPIPE 13      /* end of pipe */
0127: 
0128: /* tunable variables */
0129: 
0130: #define NBUF    15              /* size of buffer cache */
0131: #define NINODE  100             /* number of in core inodes */
0132: #define NFILE   100             /* number of in core file structures */
0133: #define NMOUNT  5               /* number of mountable file systems */
0134: #define NEXEC   3               /* number of simultaneous exec's */
0135: #define MAXMEM  (64*32)         /* max core per process 
0136:                                                 - first # is Kw */
0137: #define SSIZE   20              /* initial stack size (*64 bytes) */
0138: #define SINCR   20              /* increment of stack (*64 bytes) */
0139: #define NOFILE  15              /* max open files per process */
0140: #define CANBSIZ 256             /* max size of typewriter line */
0141: #define CMAPSIZ 100             /* size of core allocation area */
0142: #define SMAPSIZ 100             /* size of swap allocation area */
0143: #define NCALL   20              /* max simultaneous time callouts */
0144: #define NPROC   50              /* max number of processes */
0145: #define NTEXT   40              /* max number of pure texts */
0146: #define NCLIST  100             /* max total clist size */
0147: #define HZ      60              /* Ticks/second of the clock */
0148: 
0149: 
0150: 
0151: /* priorities: probably should not be altered too much */
0152: 
0153: 
0154: #define PSWP    -100
0155: #define PINOD   -90
0156: #define PRIBIO  -50
0157: #define PPIPE   1
0158: #define PWAIT   40
0159: #define PSLEP   90
0160: #define PUSER   100
0161: 
0162: /* Certain processor registers */
0163: 
0164: #define PS      0177776
0165: #define KL      0177560
0166: #define SW      0177570
0167: 
0168: /* ---------------------------       */
0169: 
0170: /* structure to access : */
0171: 
0172: 
0173:    /* an integer */
0174: 
0175: struct {   int   integ;   };
0176: 
0177: 
0178:    /* an integer in bytes */
0179: 
0180: struct {   char lobyte;   char hibyte;   };
0181: 
0182: 
0183:    /* a sequence of integers */
0184: 
0185: struct {   int   r[];   };
0186: 
0187: 
0188: /* ---------------------------       */

0200: /* Random set of variables used by more than one routine. */
0201: 
0202: char    canonb[CANBSIZ];        /* buffer for erase and kill (#@) */
0203: int     coremap[CMAPSIZ];       /* space for core allocation */
0204: int     swapmap[SMAPSIZ];       /* space for swap allocation */
0205: 
0206: int     *rootdir;               /* pointer to inode of root directory */
0207: 
0208: int     cputype;                /* type of cpu =40, 45, or 70 */
0209: 
0210: int     execnt;                 /* number of processes in exec */
0211: 
0212: int     lbolt;                  /* time of day in 60th not in time */
0213: int     time[2];                /* time in sec from 1970 */
0214: int     tout[2];                /* time of day of next sleep */
0215: 
0216: int     mpid;                   /* generic for unique process id's */
0217: 
0218: char    runin;                  /* scheduling flag */
0219: char    runout;                 /* scheduling flag */
0220: char    runrun;                 /* scheduling flag */
0221: 
0222: char    curpri;                 /* more scheduling */
0223: 
0224: int     maxmem;                 /* actual max memory per process */
0225: 
0226: int     *lks;                   /* pointer to clock device */
0227: 
0228: int     rootdev;                /* dev of root see conf.c */
0229: int     swapdev;                /* dev of swap see conf.c */
0230: 
0231: int     swplo;                  /* block number of swap space */
0232: int     nswap;                  /* size of swap space */
0233: 
0234: int     updlock;                /* lock for sync */
0235: int     rablock;                /* block to be read ahead */
0236: 
0237: char    regloc[];               /* locs. of saved user registers (trap.c) */
0238: 
0239: 
0240: 
0241: /* ---------------------------       */
0242: 
0243: 
0244: 
0245: 
0246: 
0247: 
0248: 
0249: 
0250: 
0251: /* ---------------------------       */
0252: 
0253: /* The callout structure is for a routine
0254:  * arranging to be called by the clock interrupt
0255:  * (clock.c) with a specified argument,
0256:  * within a specified amount of time.
0257:  * It is used, for example, to time tab delays
0258:  * on teletypes. */
0259: 
0260: struct  callo
0261: {
0262:         int     c_time;         /* incremental time */
0263:         int     c_arg;          /* argument to routine */
0264:         int     (*c_func)();    /* routine */
0265: } callout[NCALL];
0266: /* ---------------------------       */
0267: 
0268: /* Mount structure.
0269:  * One allocated on every mount. Used to find the super block.
0270:  */
0271: 
0272: struct  mount
0273: {
0274:         int     m_dev;          /* device mounted */
0275:         int     *m_bufp;        /* pointer to superblock */
0276:         int     *m_inodp;       /* pointer to mounted on inode */
0277: } mount[NMOUNT];
0278: /* ---------------------------       */

0300: 
0301: /* KT-11 addresses and bits */
0302: 
0303: #define UISD    0177600         /* first user I-space descriptor register */
0304: 
0305: #define UISA    0177640         /* first user I-space address register */
0306: 
0307: #define UDSA    0177660         /* first user D-space address register */
0308: 
0309: 
0310: #define UBMAP   0170200         /* address to access 11/70 UNIBUS map */
0311: 
0312: #define RO      02              /* access abilities */
0313: #define WO      04
0314: #define RW      06
0315: #define ED      010             /* extend direction */
0316: 
0317: /* ---------------------------       */
0318: 
0319: int     *ka6;           /* 11/40 KISA6; 11/45 KDSA6 */

0350: /*
0351:  * One structure allocated per active
0352:  * process. It contains all data needed
0353:  * about the process while the
0354:  * process may be swapped out.
0355:  * Other per process data (user.h)
0356:  * is swapped with the process.
0357:  */
0358: struct  proc
0359: {
0360:         char    p_stat;
0361:         char    p_flag;
0362:         char    p_pri;          /* priority, negative is high */
0363:         char    p_sig;          /* signal number sent to this process */
0364:         char    p_uid;          /* user id, used to direct tty signals */
0365:         char    p_time;         /* resident time for scheduling */
0366:         char    p_cpu;          /* cpu usage for scheduling */
0367:         char    p_nice;         /* nice for scheduling */
0368:         int     p_ttyp;         /* controlling tty */
0369:         int     p_pid;          /* unique process id */
0370:         int     p_ppid;         /* process id of parent */
