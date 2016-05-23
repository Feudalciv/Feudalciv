name=$1
uppercase=${name^}
citystyle=$2
flag=$3

echo "[nation_$name]

name=_(\"$uppercase\")
plural=_(\"?plural:$uppercase\")
groups= \"Feudalciv\"
legend=_(\"\")

leaders = {
 \"name\",                        \"sex\"
}

ruler_titles = {
 \"government\",      \"male_title\",           \"female_title\"
}

flag=\"$flag\"
flag_alt = \"united_kingdom\"
city_style = \"$citystyle\"

init_techs=\"\"
init_buildings=\"\"
init_units=\"\"

" > $name.ruleset
