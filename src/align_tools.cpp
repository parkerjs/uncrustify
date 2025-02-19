/**
 * @file align_tools.cpp
 *
 * @author  Guy Maurel
 * split from align.cpp
 * @author  Ben Gardner
 * @license GPL v2+
 */

#include "align_tools.h"

#include "space.h"
#include "uncrustify.h"


Chunk *skip_c99_array(Chunk *sq_open)
{
   if (chunk_is_token(sq_open, CT_SQUARE_OPEN))
   {
      Chunk *tmp = sq_open->SkipToMatch()->GetNextNc();

      if (tmp->Is(CT_ASSIGN))
      {
         return(tmp->GetNextNc());
      }
   }
   return(nullptr);
} // skip_c99_array


Chunk *scan_ib_line(Chunk *start, bool first_pass)
{
   UNUSED(first_pass);
   LOG_FUNC_ENTRY();
   Chunk  *prev_match = nullptr;
   size_t idx         = 0;

   // Skip past C99 "[xx] =" stuff
   Chunk *tmp = skip_c99_array(start);

   if (tmp != nullptr)
   {
      set_chunk_parent(start, CT_TSQUARE);
      start            = tmp;
      cpd.al_c99_array = true;
   }
   Chunk *pc = start;

   if (pc != nullptr)
   {
      LOG_FMT(LSIB, "%s(%d): start: orig_line is %zu, orig_col is %zu, column is %zu, type is %s\n",
              __func__, __LINE__, pc->orig_line, pc->orig_col, pc->column, get_token_name(pc->type));
   }
   else
   {
      pc = Chunk::NullChunkPtr;
   }

   while (  pc->IsNotNullChunk()
         && !pc->IsNewline()
         && pc->level >= start->level)
   {
      //LOG_FMT(LSIB, "%s:     '%s'   col %d/%d line %zu\n", __func__,
      //        pc->Text(), pc->column, pc->orig_col, pc->orig_line);

      Chunk *next = pc->GetNext();

      if (  next->IsNullChunk()
         || next->IsComment())
      {
         // do nothing
      }
      else if (  chunk_is_token(pc, CT_ASSIGN)
              || chunk_is_token(pc, CT_BRACE_OPEN)
              || chunk_is_token(pc, CT_BRACE_CLOSE)
              || chunk_is_token(pc, CT_COMMA))
      {
         size_t token_width = space_col_align(pc, next);

         // TODO: need to handle missing structure defs? ie NULL vs { ... } ??

         // Is this a new entry?
         if (idx >= cpd.al_cnt)
         {
            if (idx == 0)
            {
               LOG_FMT(LSIB, "%s(%d): Prepare the 'idx's\n", __func__, __LINE__);
            }
            LOG_FMT(LSIB, "%s(%d):   New idx is %2.1zu, pc->column is %2.1zu, Text() '%s', token_width is %zu, type is %s\n",
                    __func__, __LINE__, idx, pc->column, pc->Text(), token_width, get_token_name(pc->type));
            cpd.al[cpd.al_cnt].type = pc->type;
            cpd.al[cpd.al_cnt].col  = pc->column;
            cpd.al[cpd.al_cnt].len  = token_width;
            cpd.al_cnt++;

            if (cpd.al_cnt == uncrustify::limits::AL_SIZE)
            {
               fprintf(stderr, "Number of 'entry' to be aligned is too big for the current value %d,\n",
                       uncrustify::limits::AL_SIZE);
               fprintf(stderr, "at line %zu, column %zu.\n",
                       pc->orig_line, pc->orig_col);
               fprintf(stderr, "Please make a report.\n");
               log_flush(true);
               exit(EX_SOFTWARE);
            }
            idx++;
         }
         else
         {
            // expect to match stuff
            if (cpd.al[idx].type == pc->type)
            {
               LOG_FMT(LSIB, "%s(%d):   Match? idx is %2.1zu, orig_line is %2.1zu, column is %2.1zu, token_width is %zu, type is %s\n",
                       __func__, __LINE__, idx, pc->orig_line, pc->column, token_width, get_token_name(pc->type));

               // Shift out based on column
               if (prev_match == nullptr)
               {
                  if (pc->column > cpd.al[idx].col)
                  {
                     LOG_FMT(LSIB, "%s(%d): [ pc->column (%zu) > cpd.al[%zu].col(%zu) ] \n",
                             __func__, __LINE__, pc->column, idx, cpd.al[idx].col);

                     ib_shift_out(idx, pc->column - cpd.al[idx].col);
                     cpd.al[idx].col = pc->column;
                  }
               }
               else if (idx > 0)
               {
                  LOG_FMT(LSIB, "%s(%d):   prev_match '%s', prev_match->orig_line is %zu, prev_match->orig_col is %zu\n",
                          __func__, __LINE__, prev_match->Text(), prev_match->orig_line, prev_match->orig_col);
                  int min_col_diff = pc->column - prev_match->column;
                  int cur_col_diff = cpd.al[idx].col - cpd.al[idx - 1].col;

                  if (cur_col_diff < min_col_diff)
                  {
                     LOG_FMT(LSIB, "%s(%d):   pc->orig_line is %zu\n",
                             __func__, __LINE__, pc->orig_line);
                     ib_shift_out(idx, min_col_diff - cur_col_diff);
                  }
               }
               LOG_FMT(LSIB, "%s(%d): at ende of the loop: now is col %zu, len is %zu\n",
                       __func__, __LINE__, cpd.al[idx].col, cpd.al[idx].len);
               idx++;
            }
         }
         prev_match = pc;
      }
      pc = pc->GetNextNc();
   }
   return(pc);
} // scan_ib_line


void ib_shift_out(size_t idx, size_t num)
{
   while (idx < cpd.al_cnt)
   {
      cpd.al[idx].col += num;
      idx++;
   }
} // ib_shift_out


Chunk *step_back_over_member(Chunk *pc)
{
   if (pc == nullptr)
   {
      pc = Chunk::NullChunkPtr;
   }
   Chunk *tmp = pc->GetPrevNcNnl();

   // Skip over any class stuff: bool CFoo::bar()
   while (  tmp->IsNotNullChunk()
         && chunk_is_token(tmp, CT_DC_MEMBER))
   {
      pc  = tmp->GetPrevNcNnl();
      tmp = pc->GetPrevNcNnl();
   }
   return(pc);
} // step_back_over_member
