#!/bin/bash

if ! test -e ./morla; then
  echo "./morla doesn't exit!"
  exit;
fi

echo -n "gchar morla_rdfs[] =" > src/morla.h
cat ./morla | sed -e 's/"/\\"/g' -e 's/$/\\n"/g' -e 's/^/"/g' >> src/morla.h
echo ";" >> src/morla.h
