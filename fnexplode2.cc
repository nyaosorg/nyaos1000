int fnexplode2(struct filelist *&list   , int &count ,
	       struct filelist *&dirlist, int &dircount ,
	       int &max_length ,
	       const char *path )
{
  char dir[ FILENAME_MAX ]    , *dir_p    = dir;
  char rawdir[ FILENAME_MAX ] , *rawdir_p = rawdir;
  char fname[ FILENAME_MAX ]  , *fname_p  = fname;
  
  const char *lastroot=NULL;
  if( path[0]=='~' ){
    lastroot = path;
  }
  for(const char *sp=path ; *sp != '\0' ; sp++ ){
    if( is_kanji(*sp) ){
      ++sp;
    }else if( *sp=='\\' || *sp=='/' || *sp==':' ){
      lastroot = sp;
    }
  }
  
  const char *p=path;
  if( lastroot != NULL ){
    while( p <= lastroot ){
      *rawdir_p++ = *p;
      *dir_p++ = *p++;
    }
  }
  /* '.'を付けることで 末尾が ':','/'でも有効に働く (^_^) */
  *dir_p++ = '.';
  *dir_p   = '\0';
  *rawdir_p = '\0';

  while( *p != '\0' )
    *fname_p++ = *p++;
  *fname_p = '\0';

  if( fname[0] == '\0' ){
    struct filelist *tmp =
      (struct filelist *)malloc(sizeof(struct filelist)	+ (dir_p-dir) );
    const char *sp=dir;
    char *dp=tmp->name;
    while( sp < dir_p )
      *dp++ = *sp++;
    *dp = '\0';
    dirlist = fsort_and_insert(dirlist,tmp,&dircount);
    return 0;
  }

  DIR *dirp=opendir(dir);
  if( dirp == NULL )
    return -1;
  
  struct dirent *dirbuf;
  while( (dirbuf=readdir(dirp)) != NULL ){
    if( _fnmatch( fname , dirbuf->d_name , _FNM_OS2 | _FNM_IGNORECASE )!=0 )
      continue;

    struct filelist *tmp =
      (struct filelist *)malloc(sizeof(struct filelist)
				+ dirbuf->d_namlen
				+ (rawdir_p-rawdir)
				);
    char *dp=tmp->name;
    /** ディレクトリ部をコピ－ **/
    const char *sp=rawdir;
    while( sp < rawdir_p )
      *dp++ = *sp++;

    /** ファイル名部をコピ－ **/
    sp = dirbuf->d_name;
    for(int i=0; i<dirbuf->d_namlen ; i++ )
      *dp++ = *sp++;
    *dp = '\0';
    
    tmp->length = dirbuf->d_namlen + (rawdir_p-rawdir) ;

    tmp->date   = dirbuf->d_date;
    tmp->time   = dirbuf->d_time;
    tmp->attr   = dirbuf->d_attr;
    tmp->size   = dirbuf->d_size;

    if( tmp->attr & A_DIR ){
      dirlist = fsort_and_insert(dirlist,tmp,&dircount);
    }else{
      list = fsort_and_insert(list,tmp,&count);
      if( tmp->length > max_length )
	max_length = tmp->length;
    }
  }
  closedir(dirp);
  return 0;
}
#endif

#elif 0
      }else{
	/* オプションでない文字列 ... ファイル名 */
	char **list=_fnexplode(argv[i]);
	if( list != NULL ){
	  for(char **ptr=list; *ptr != NULL ; ptr++ ){

	    struct stat stbuf;
	    int len=strlen(*ptr);

	    if( stat( *ptr , &stbuf ) == 0 ){
	      struct filelist *node=
		(struct filelist*)alloca(sizeof(struct filelist)+len);
	      strcpy( node->name , *ptr );
	      node->attr   = stbuf.st_attr;
	      node->length = len;
	      node->size   = stbuf.st_size;
	      
	      struct tm *tmbuf=localtime(&stbuf.st_mtime);
	      node->t.second = tmbuf->tm_sec/2;   /* 0..59 --> 0..29  */
	      node->t.minute = tmbuf->tm_min;     /* 0..59  */
	      node->t.hour   = tmbuf->tm_hour;    /* 0..23  */
	      node->d.day    = tmbuf->tm_mday;    /* 1..31  */
	      node->d.month  = tmbuf->tm_mon+1;   /* 0..11 --> 1..12   */
	      node->d.year   = tmbuf->tm_year-80; /* 0:1900 --> 0:1980 */
	      
	      if( stbuf.st_attr & A_DIR ){
		dirs  = fsort_and_insert(dirs ,node,&dircount);
	      }else{
		files = fsort_and_insert(files,node,&filecount);
		if( len > max_length )
		  max_length = len;
	      }
	    }else{
	      fprintf(stderr,"%s: no such file or directory.\n",argv[i]);
	      filefault++;
	    }
	  }
	  _fnexplodefree(list);
	}else{
	  struct stat stbuf;
	  int len=strlen(argv[i]);
	  char *fn=argv[i];

	  /*「ls A:」にも対応させるため、ドットを末尾に追加する。*/
	  if( argv[i][1]==':' && argv[i][2]=='\0' ){
	    static char drv[]="@:.";
	    drv[0]=argv[i][0];
	    fn = drv;
	  }

	  if( stat( fn , &stbuf ) == 0 ){
	    struct filelist *node=
	      (struct filelist*)alloca(sizeof(struct filelist)+len);
	    strcpy( node->name , argv[i] );
	    node->attr   = stbuf.st_attr;
	    node->length = len;
	    node->size   = stbuf.st_size;
	      
	    struct tm *tmbuf=localtime(&stbuf.st_mtime);
	    node->t.second = tmbuf->tm_sec/2;   /* 0..59  --> 0..29 */
	    node->t.minute = tmbuf->tm_min;     /* 0..59  */
	    node->t.hour   = tmbuf->tm_hour;    /* 0..23  */
	    node->d.day    = tmbuf->tm_mday;    /* 1..31  */
	    node->d.month  = tmbuf->tm_mon+1;   /* 0..11  --> 1..12  */
	    node->d.year   = tmbuf->tm_year-80; /* 0:1900 --> 0:1980 */
	    
	    if( stbuf.st_attr & A_DIR ){
	      dirs  = fsort_and_insert(dirs ,node,&dircount);
	    }else{
	      files = fsort_and_insert(files,node,&filecount);
	      if( len > max_length )
		max_length = len;
	    }
	  }else{
	    fprintf(stderr,"%s: no such file or directory\n",argv[i]);
	    filefault++;
	  }
	}
      }/* argv loop */
#else
      }else if(fnexplode2(files, filecount, dirs, dircount,
			  max_length,argv[i]) !=0 ){
	/* オプションでない文字列 ... ファイル名 */
	fprintf(stderr,"%s: no such file or directory.\n",argv[i]);
	filefault++;
      }/* argv loop */
#endif
