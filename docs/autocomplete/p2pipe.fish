function _p2pipe_cmd_0
    set 1 $argv[1]
    __fish_complete_path "$1"
end

function __complgen_match
    set prefix $argv[1]

    set candidates
    set descriptions
    while read c
        set a (string split --max 1 -- "	" $c)
        set --append candidates $a[1]
        if set --query a[2]
            set --append descriptions $a[2]
        else
            set --append descriptions ""
        end
    end

    if test -z "$candidates"
        return 1
    end

    set escaped_prefix (string escape --style=regex -- $prefix)
    set regex "^$escaped_prefix.*"

    set matches_case_sensitive
    set descriptions_case_sensitive
    for i in (seq 1 (count $candidates))
        if string match --regex --quiet --entire -- $regex $candidates[$i]
            set --append matches_case_sensitive $candidates[$i]
            set --append descriptions_case_sensitive $descriptions[$i]
        end
    end

    if set --query matches_case_sensitive[1]
        for i in (seq 1 (count $matches_case_sensitive))
            printf '%s	%s\n' $matches_case_sensitive[$i] $descriptions_case_sensitive[$i]
        end
        return 0
    end

    set matches_case_insensitive
    set descriptions_case_insensitive
    for i in (seq 1 (count $candidates))
        if string match --regex --quiet --ignore-case --entire -- $regex $candidates[$i]
            set --append matches_case_insensitive $candidates[$i]
            set --append descriptions_case_insensitive $descriptions[$i]
        end
    end

    if set --query matches_case_insensitive[1]
        for i in (seq 1 (count $matches_case_insensitive))
            printf '%s	%s\n' $matches_case_insensitive[$i] $descriptions_case_insensitive[$i]
        end
        return 0
    end

    return 1
end


function _p2pipe
    set COMP_LINE (commandline --cut-at-cursor)

    set COMP_WORDS
    echo $COMP_LINE | read --tokenize --array COMP_WORDS
    if string match --quiet --regex '.*\s$' $COMP_LINE
        set COMP_CWORD (math (count $COMP_WORDS) + 1)
    else
        set COMP_CWORD (count $COMP_WORDS)
    end

    set literals serve -P --port --help listen -I --ip -C --capacity -d --dst -i --id talk -s --src -h --help -v --version

    set descrs
    set descrs[1] "Act as a server"
    set descrs[2] "Specify the port of the server"
    set descrs[3] "Prints the help message"
    set descrs[4] "Act as a receiver"
    set descrs[5] "Specify the IP of the server"
    set descrs[6] "Specify the buffer capacity"
    set descrs[7] "Specify the destination file"
    set descrs[8] "Specify the session id"
    set descrs[9] "Act as a sender"
    set descrs[10] "Specify the source file"
    set descrs[11] "Prints the project version"
    set descr_literal_ids 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 19
    set descr_ids 1 2 2 3 4 5 5 6 6 7 7 8 8 9 10 10 3 11
    set regexes 
    set literal_transitions_inputs
    set nontail_transitions
    set literal_transitions_inputs[1] "1 5 14 17 18 19 20"
    set literal_transitions_tos[1] "2 3 4 5 5 5 5"
    set literal_transitions_inputs[2] "2 3 4"
    set literal_transitions_tos[2] "6 6 2"
    set literal_transitions_inputs[3] "2 3 4 6 7 8 9 10 11 12 13"
    set literal_transitions_tos[3] "11 11 3 12 12 13 13 14 14 15 15"
    set literal_transitions_inputs[4] "2 3 4 6 7 8 9 15 16"
    set literal_transitions_tos[4] "7 7 4 8 8 9 9 10 10"
    set literal_transitions_inputs[5] "17 18 19 20"
    set literal_transitions_tos[5] "5 5 5 5"

    set match_anything_transitions_from 11 12 13 8 15 7 9 10 14 6
    set match_anything_transitions_to 3 3 3 4 3 4 4 4 3 2

    set state 1
    set word_index 2
    while test $word_index -lt $COMP_CWORD
        set -- word $COMP_WORDS[$word_index]

        if set --query literal_transitions_inputs[$state] && test -n $literal_transitions_inputs[$state]
            set inputs (string split ' ' $literal_transitions_inputs[$state])
            set tos (string split ' ' $literal_transitions_tos[$state])

            set literal_id (contains --index -- "$word" $literals)
            if test -n "$literal_id"
                set index (contains --index -- "$literal_id" $inputs)
                set state $tos[$index]
                set word_index (math $word_index + 1)
                continue
            end
        end

        if set --query match_anything_transitions_from[$state] && test -n $match_anything_transitions_from[$state]
            set index (contains --index -- "$state" $match_anything_transitions_from)
            set state $match_anything_transitions_to[$index]
            set word_index (math $word_index + 1)
            continue
        end

        return 1
    end

    set literal_froms_level_0 5 1 4 3 2
    set literal_inputs_level_0 "17 19|1 5 14 17 19|2 3 4 6 7 8 9 15 16|2 3 4 6 7 8 9 10 11 12 13|2 3 4"
    set literal_froms_level_1 1 5
    set literal_inputs_level_1 "18 20|18 20"
    set nontail_command_froms_level_0 
    set nontail_commands_level_0 
    set nontail_regexes_level_0 
    set nontail_command_froms_level_1 
    set nontail_commands_level_1 
    set nontail_regexes_level_1 
    set command_froms_level_0 10 14
    set commands_level_0 "0" "0"
    set command_froms_level_1 
    set commands_level_1 

    for fallback_level in (seq 0 1)
        set candidates
        set froms_name literal_froms_level_$fallback_level
        set froms $$froms_name
        set index (contains --index -- "$state" $froms)
        if test -n "$index"
            set level_inputs_name literal_inputs_level_$fallback_level
            set input_assoc_values (string split '|' $$level_inputs_name)
            set state_inputs (string split ' ' $input_assoc_values[$index])
            for literal_id in $state_inputs
                set descr_index (contains --index -- "$literal_id" $descr_literal_ids)
                if test -n "$descr_index"
                    set --append candidates (printf '%s\t%s\n' $literals[$literal_id] $descrs[$descr_ids[$descr_index]])
                else
                    set --append candidates (printf '%s\n' $literals[$literal_id])
                end
            end
        end

        set commands_name command_froms_level_$fallback_level
        set commands $$commands_name
        set index (contains --index -- "$state" $commands)
        if test -n "$index"
            set commands_name commands_level_$fallback_level
            set commands (string split ' ' $$commands_name)
            set function_id $commands[$index]
            set function_name _p2pipe_cmd_$function_id
            set --append candidates ($function_name "$COMP_WORDS[$COMP_CWORD]")
        end
        printf '%s\n' $candidates | __complgen_match $COMP_WORDS[$word_index] && return 0
    end
end

complete --erase p2pipe
complete --command p2pipe --no-files --arguments "(_p2pipe)"
