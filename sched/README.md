# sched

Repositorio para el esqueleto del [TP sched](https://fisop.github.io/website/tps/sched) del curso Mendez-Fresia de **Sistemas Operativos (7508) - FIUBA**

## Respuestas teóricas

Utilizar el archivo `sched.md` provisto en el repositorio

## Compilar

```bash
make
```

## Pruebas

* Para correr las pruebas con round robin:
```bash
make -B -e USE_ROUND_ROBIN=true grade
```
* Para correr las pruebas con MLFQ:
```bash
make -B -e USE_MLFQ=true grade
```

## Linter

```bash
$ make format
```

Para efectivamente subir los cambios producidos por el `format`, hay que `git add .` y `git commit`.
