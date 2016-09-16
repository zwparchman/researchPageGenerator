      PROGRAM MAIN
      INTEGER no_args, i
      CHARACTER argv*255

      no_args = IARGC()

      DO i=0, no_args
        CALL GETARG(i, ARGV)
        IF ( ARGV .EQ. "=recover" ) THEN
          IF ( i .EQ. 1 ) THEN
            CALL ckpt_setup(1, 0)
          ELSE
            WRITE (*,*) '=recover, if used, must be only argument'
            GO TO 10
          END IF
        END IF
      END DO

      CALL ckpt_setup( 0, 0)

      CALL CKPT_TARGET()
10    CONTINUE

      END
