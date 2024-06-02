#include <kernel.h>

extern char *getwd();
extern char *xmalloc();

COMMAND commands[] = {
    {"EJECUTAR_SCRIPT", ejecutar_script, "Abrir archivo de comandos a ejecutar"},
    {"INICIAR_PROCESO", iniciar_proceso, "Crea PCB en estado NEW"},
    {"FINALIZAR_PROCESO", finalizar_proceso, "Finaliza proceso del sistema"},
    {"DETENER_PLANIFICACION", detener_planificacion, "Pausar planificacion a corto y largo plazo"},
    {"INICIAR_PLANIFICACION", iniciar_planificacion, "Reanuda la planificacion a corto y largo plazo"},
    {"MULTIPROGRAMACION", multiprogramacion, "Modifica el grado de multiprogramacion por el valor dado"},
    {"PROCESO_ESTADO", proceso_estado, "Lista procesos por estado en la consola"},
    {"INTERFACES_CONECTADAS", interfaces_conectadas, "Lista de interfaces conectadas"},
    {NULL, NULL, NULL}};

/* When non-zero, this global means the user is done using this program. */
int done;

char *dupstr(char *s)
{
  char *r;

  r = xmalloc(strlen(s) + 1);
  strcpy(r, s);
  return (r);
}

/* Execute a command line. */
int execute_line(char *line, t_log *logger)
{
  int i = 0;
  int j = 0;
  COMMAND *command;
  char param[266];
  char word[30];

  if (line[i] != '\0')
  {
    while (line[i] && !isspace(line[i]))
    {
      word[i] = line[i];
      i++;
    }
    word[i] = '\0';

    while (line[i] && isspace(line[i]))
    {
      i++;
    }

    while (line[i] && line[i] != '\0')
    {
      param[j] = line[i];
      i++;
      j++;
    }
    param[j] = '\0';

    log_info(logger, "Command: %s\tParameter: %s\n", word, param);

    command = find_command(word);

    if (!command)
    {
      log_error(logger, "%s: No se pudo encotrar ese comando\n", word);
      return (-1);
    }

    return ((*(command->func))(param));
  }
  else
  {
    log_info(logger, "Fin de archivo.");
    return 0;
  }
}

/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */
COMMAND *find_command(char *name)
{
  register int i;

  for (i = 0; commands[i].name; i++)
    if (strcmp(name, commands[i].name) == 0)
      return (&commands[i]);

  return ((COMMAND *)NULL);
}

/* Strip whitespace from the start and end of STRING.  Return a pointer
   into STRING. */
char *stripwhite(char *string)
{
  register char *s, *t;

  for (s = string; whitespace(*s); s++)
    ;

  if (*s == 0)
  {
    return (s);
  }
  else
  {
    t = s + strlen(s) - 1;
    while (t > s && whitespace(*t))
    {
      t--;
    }
    *++t = '\0';
    return s;
  }
}