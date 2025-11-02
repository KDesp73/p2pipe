#compdef p2pipe

_p2pipe_cmd_0 () {
    _path_files
}

_p2pipe () {
    declare -a literals=("serve" "--port" "listen" "--ip" "--help" "--dst" "talk" "--src" "-h" "--help" "-v" "--version")
    declare -A descrs=()
    descrs[0]="Act as a server"
    descrs[1]="Specify the port of the server"
    descrs[2]="Act as a receiver"
    descrs[3]="Specify the ip of the server"
    descrs[4]="Prints the help message"
    descrs[5]="Specify the destination file"
    descrs[6]="Act as a sender"
    descrs[7]="Specify the source file"
    descrs[8]="Prints the project version"
    declare -A descr_id_from_literal_id=([1]=0 [2]=1 [3]=2 [4]=3 [5]=4 [6]=5 [7]=6 [8]=7 [9]=4 [11]=8)
    declare -a regexes=()
    declare -A literal_transitions=()
    literal_transitions[1]="([1]=2 [3]=3 [7]=4 [9]=5 [10]=5 [11]=5 [12]=5)"
    literal_transitions[2]="([2]=5)"
    literal_transitions[3]="([2]=5 [4]=5 [5]=5 [6]=6)"
    literal_transitions[4]="([2]=5 [4]=5 [5]=5 [8]=6)"
    declare -A nontail_transitions=()
    declare -A match_anything_transitions=([6]=5)
    declare -A subword_transitions=()

    declare state=1
    declare word_index=2
    while [[ $word_index -lt $CURRENT ]]; do
        if [[ -v "literal_transitions[$state]" ]]; then
            eval "declare -A state_transitions=${literal_transitions[$state]}"

            declare word=${words[$word_index]}
            declare word_matched=0
            for ((literal_id = 1; literal_id <= $#literals; literal_id++)); do
                if [[ ${literals[$literal_id]} = "$word" ]]; then
                    if [[ -v "state_transitions[$literal_id]" ]]; then
                        state=${state_transitions[$literal_id]}
                        word_index=$((word_index + 1))
                        word_matched=1
                        break
                    fi
                fi
            done
            if [[ $word_matched -ne 0 ]]; then
                continue
            fi
        fi

        if [[ -v "nontail_transitions[$state]" ]]; then
            eval "declare -A state_nontails=${nontail_transitions[$state]}"
            declare nontail_matched=0
            for regex_id in "${(k)state_nontails}"; do
                declare regex="^(${regexes[$regex_id]}).*"
                if [[ ${subword} =~ $regex && -n ${match[1]} ]]; then
                    match="${match[1]}"
                    match_len=${#match}
                    char_index=$((char_index + match_len))
                    state=${state_nontails[$regex_id]}
                    nontail_matched=1
                    break
                fi
            done
            if [[ $nontail_matched -ne 0 ]]; then
                continue
            fi
        fi


        if [[ -v "match_anything_transitions[$state]" ]]; then
            state=${match_anything_transitions[$state]}
            word_index=$((word_index + 1))
            continue
        fi

        return 1
    done

    declare -A literal_transitions_level_0=([1]="1 3 7 9 11" [4]="2 4 5 8" [3]="2 4 5 6" [2]="2")
    declare -A literal_transitions_level_1=([1]="10 12")
    declare -A subword_transitions_level_0=()
    declare -A subword_transitions_level_1=()
    declare -A commands_level_0=()
    declare -A commands_level_1=()
    declare -A nontail_commands_level_0=()
    declare -A nontail_regexes_level_0=()
    declare -A nontail_commands_level_1=()
    declare -A nontail_regexes_level_1=()
    declare -A specialized_commands_level_0=([6]="0")
    declare -A specialized_commands_level_1=()

    declare max_fallback_level=1
    for (( fallback_level=0; fallback_level <= max_fallback_level; fallback_level++ )); do
        completions_no_description_trailing_space=()
        completions_no_description_no_trailing_space=()
        completions_trailing_space=()
        suffixes_trailing_space=()
        descriptions_trailing_space=()
        completions_no_trailing_space=()
        suffixes_no_trailing_space=()
        descriptions_no_trailing_space=()
        matches=()

        declare literal_transitions_name=literal_transitions_level_${fallback_level}
        eval "declare initializer=\${${literal_transitions_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for literal_id in "${transitions[@]}"; do
            if [[ -v "descr_id_from_literal_id[$literal_id]" ]]; then
                declare descr_id=$descr_id_from_literal_id[$literal_id]
                completions_trailing_space+=("${literals[$literal_id]}")
                suffixes_trailing_space+=("${literals[$literal_id]}")
                descriptions_trailing_space+=("${descrs[$descr_id]}")
            else
                completions_no_description_trailing_space+=("${literals[$literal_id]}")
            fi
        done

        declare subword_transitions_name=subword_transitions_level_${fallback_level}
        eval "declare initializer=\${${subword_transitions_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for subword_id in "${transitions[@]}"; do
            _p2pipe_subword_${subword_id} complete "${words[$CURRENT]}"
            completions_no_description_trailing_space+=("${subword_completions_no_description_trailing_space[@]}")
            completions_trailing_space+=("${subword_completions_trailing_space[@]}")
            completions_no_trailing_space+=("${subword_completions_no_trailing_space[@]}")
            suffixes_no_trailing_space+=("${subword_suffixes_no_trailing_space[@]}")
            suffixes_trailing_space+=("${subword_suffixes_trailing_space[@]}")
            descriptions_trailing_space+=("${subword_descriptions_trailing_space[@]}")
            descriptions_no_trailing_space+=("${subword_descriptions_no_trailing_space[@]}")
        done

        declare commands_name=commands_level_${fallback_level}
        eval "declare initializer=\${${commands_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for command_id in "${transitions[@]}"; do
            declare output=$(_p2pipe_cmd_${command_id} "${words[$CURRENT]}")
            declare -a command_completions=("${(@f)output}")
            for line in ${command_completions[@]}; do
                declare parts=(${(@s:	:)line})
                if [[ -v "parts[2]" ]]; then
                    completions_trailing_space+=("${parts[1]}")
                    suffixes_trailing_space+=("${parts[1]}")
                    descriptions_trailing_space+=("${parts[2]}")
                else
                    completions_no_description_trailing_space+=("${parts[1]}")
                fi
            done
        done

        declare commands_name=nontail_commands_level_${fallback_level}
        eval "declare command_initializer=\${${commands_name}[$state]}"
        eval "declare -a command_transitions=($command_initializer)"
        declare regexes_name=nontail_regexes_level_${fallback_level}
        eval "declare regexes_initializer=\${${regexes_name}[$state]}"
        eval "declare -a regexes_transitions=($regexes_initializer)"
        for (( i=1; i <= ${#command_transitions[@]}; i++ )); do
            declare command_id=${command_transitions[$i]}
            declare regex_id=${regexes_transitions[$i]}
            declare regex="^(${regexes[$regex_id]}).*"
            declare output=$(_p2pipe_cmd_${command_id} "${words[$CURRENT]}")
            declare -a command_completions=("${(@f)output}")
            for line in ${command_completions[@]}; do
                declare parts=(${(@s:	:)line})
                if [[ ${parts[1]} =~ $regex && -n ${match[1]} ]]; then
                    parts[1]=${match[1]}
                    if [[ -v "parts[2]" ]]; then
                        completions_trailing_space+=("${parts[1]}")
                        suffixes_trailing_space+=("${parts[1]}")
                        descriptions_trailing_space+=("${parts[2]}")
                    else
                        completions_no_description_trailing_space+=("${parts[1]}")
                    fi
                fi
            done
        done

        declare specialized_commands_name=specialized_commands_level_${fallback_level}
        eval "declare initializer=\${${specialized_commands_name}[$state]}"
        eval "declare -a transitions=($initializer)"
        for command_id in "${transitions[@]}"; do
            _p2pipe_cmd_${command_id} ${words[$CURRENT]}
        done

        declare maxlen=0
        for suffix in ${suffixes_trailing_space[@]}; do
            if [[ ${#suffix} -gt $maxlen ]]; then
                maxlen=${#suffix}
            fi
        done
        for suffix in ${suffixes_no_trailing_space[@]}; do
            if [[ ${#suffix} -gt $maxlen ]]; then
                maxlen=${#suffix}
            fi
        done

        for ((i = 1; i <= $#suffixes_trailing_space; i++)); do
            if [[ -z ${descriptions_trailing_space[$i]} ]]; then
                descriptions_trailing_space[$i]="${(r($maxlen)( ))${suffixes_trailing_space[$i]}}"
            else
                descriptions_trailing_space[$i]="${(r($maxlen)( ))${suffixes_trailing_space[$i]}} -- ${descriptions_trailing_space[$i]}"
            fi
        done

        for ((i = 1; i <= $#suffixes_no_trailing_space; i++)); do
            if [[ -z ${descriptions_no_trailing_space[$i]} ]]; then
                descriptions_no_trailing_space[$i]="${(r($maxlen)( ))${suffixes_no_trailing_space[$i]}}"
            else
                descriptions_no_trailing_space[$i]="${(r($maxlen)( ))${suffixes_no_trailing_space[$i]}} -- ${descriptions_no_trailing_space[$i]}"
            fi
        done

        compadd -O m -a completions_no_description_trailing_space; matches+=("${m[@]}")
        compadd -O m -a completions_no_description_no_trailing_space; matches+=("${m[@]}")
        compadd -O m -a completions_trailing_space; matches+=("${m[@]}")
        compadd -O m -a completions_no_trailing_space; matches+=("${m[@]}")

        if [[ ${#matches} -gt 0 ]]; then
            compadd -Q -a completions_no_description_trailing_space
            compadd -Q -S ' ' -a completions_no_description_no_trailing_space
            compadd -l -Q -a -d descriptions_trailing_space completions_trailing_space
            compadd -l -Q -S '' -a -d descriptions_no_trailing_space completions_no_trailing_space
            return 0
        fi
    done
}

if [[ $ZSH_EVAL_CONTEXT =~ :file$ ]]; then
    compdef _p2pipe p2pipe
else
    _p2pipe
fi
